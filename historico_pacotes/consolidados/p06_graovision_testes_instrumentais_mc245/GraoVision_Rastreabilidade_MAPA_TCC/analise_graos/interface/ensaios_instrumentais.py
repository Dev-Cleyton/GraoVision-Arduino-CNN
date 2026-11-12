"""Registro do ensaio instrumental AMBIENTAL do GraoVision.

Nao transmite comandos ao Arduino, nao habilita aquecimento e nao estima temperatura
externa: o operador digita a leitura observada, ou 'L'/'H' (fora de faixa).
A aquisicao do Arduino continua sob responsabilidade do fluxo existente e seguro.
"""
from __future__ import annotations

import csv
from datetime import datetime
import json
from pathlib import Path
import time
import uuid

DEFAULT_OUTPUT = Path.home() / '.graovision_tcc' / 'ensaios_instrumentais'
CHECKPOINTS = (0, 30, 60, 90, 120)


def instante():
    return datetime.now().astimezone().isoformat(timespec='seconds')


def interpretar_mc245(texto):
    """MC-245: valores numericos somente 32,0..42,0 C; L/H sem leitura valida."""
    original = str(texto).strip().replace(',', '.')
    codigo = original.upper().replace('°C', '').strip()
    if codigo in ('L', 'LO', 'LOW'):
        return {'indicacao': 'L', 'temperatura_c': None, 'situacao': 'FORA_FAIXA_INFERIOR'}
    if codigo in ('H', 'HI', 'HIGH'):
        return {'indicacao': 'H', 'temperatura_c': None, 'situacao': 'FORA_FAIXA_SUPERIOR'}
    try:
        numero = float(codigo)
    except ValueError:
        raise ValueError('Digite L, H ou temperatura numérica de 32,0 a 42,0 °C')
    if not (32.0 <= numero <= 42.0):
        raise ValueError('MC-245 não mede nessa faixa. Para ambiente abaixo de 32 °C, registre L.')
    return {'indicacao': f'{numero:.1f}', 'temperatura_c': numero,
            'situacao': 'VALOR_INSTRUMENTAL_NAO_CALIBRADO'}


class EnsaioInstrumental:
    def __init__(self, agora=instante, relogio=time.monotonic):
        self._agora = agora
        self._relogio = relogio
        self.id = None
        self.ativo = False
        self.concluido = False
        self.inicio = None
        self.fim = None
        self.pontos = []
        self.pesagens = {}
        self.eventos = []
        self.posicao_passos = None
        self.status = 'AGUARDANDO'
        self.arquivos = None

    def iniciar(self):
        self.id = datetime.now().strftime('%Y%m%d_%H%M%S') + '_' + uuid.uuid4().hex[:6]
        self.ativo = True
        self.concluido = False
        self.inicio = self._agora()
        self.fim = None
        self.pontos = []
        self.pesagens = {}
        self.eventos = []
        self.posicao_passos = None
        self.status = 'INICIADO_SEM_CALOR'
        self.arquivos = None

    def receber(self, linha):
        """Somente telemetria recebida, nunca gera resposta de controle."""
        if not self.ativo:
            return None
        if linha.startswith('MEDICAO:'):
            campos = linha.split(':')
            if len(campos) != 4:
                return None
            try:
                segundo, massa, temperatura = int(campos[1]),float(campos[2]),float(campos[3])
            except ValueError:
                return None
            if not (0 <= segundo <= 600):
                return None
            # Firmware emite 121s como ultima medicao do monitoramento de 120s.
            alvo = min(CHECKPOINTS,key=lambda p:abs(segundo-p))
            if abs(segundo-alvo) > 3:
                return None
            # Protecao contra telemetria duplicada e checkpoints repetidos.
            if any(p['marco_s']==alvo for p in self.pontos):
                return None
            p = {'marco_s': alvo, 'tempo_arduino_s': segundo,
                 'ntc_c': temperatura, 'massa_g': massa,
                 'instante_arduino': self._agora(), '_relogio': self._relogio(),
                 'mc245': None, 'instante_manual': None,
                 'atraso_registro_s': None, 'observacao': ''}
            self.pontos.append(p)
            self.pontos.sort(key=lambda item:item['marco_s'])
            return ('ponto', p)
        for nome in ('P1','P2','PF','P3','P4','P5'):
            if linha.startswith(nome+':'):
                try:
                    peso = float(linha.split(':')[1])
                except (ValueError,IndexError):
                    return None
                self.pesagens[nome] = peso
                return ('peso',(nome,peso))
        if linha.startswith('PASSOS:') or linha.startswith('POS_ATUADOR:'):
            try:self.posicao_passos = int(linha.rsplit(':',1)[1])
            except (ValueError,IndexError):pass
        if linha.startswith('AQUECEDOR:ON_COMANDADO:'):
            self.eventos.append({'instante':self._agora(),'evento':'ALERTA_AQUECEDOR_COMANDADO'})
            self.status = 'ALERTA_AQUECEDOR'
            return ('alerta',linha)
        if linha == 'FIM:1':
            self.status = 'FIM_ARDUINO'
        elif linha.startswith('ERRO:'):
            self.status = 'FALHA_ARDUINO'
            self.eventos.append({'instante':self._agora(),'evento':linha})
        return None

    def pendentes(self):
        return [p for p in self.pontos if p['mc245'] is None]

    def registrar_mc245(self, marco_s, texto, observacao=''):
        if not self.ativo and not self.concluido:
            raise ValueError('Não há ensaio instrumental iniciado.')
        p = next((x for x in self.pontos if x['marco_s']==int(marco_s)),None)
        if p is None:
            raise ValueError('Esse instante ainda não foi recebido do Arduino.')
        leitura = interpretar_mc245(texto)
        p['mc245'] = leitura
        p['instante_manual'] = self._agora()
        p['atraso_registro_s'] = round(max(0,self._relogio()-p['_relogio']),1)
        p['observacao'] = str(observacao).strip()[:250]
        return p

    def finalizar(self, estado='CONCLUIDO'):
        if not self.ativo:
            return
        self.ativo = False
        self.concluido = True
        self.fim = self._agora()
        self.status = estado if self.status != 'ALERTA_AQUECEDOR' else 'ALERTA_AQUECEDOR'

    def diferencas(self):
        p = self.pesagens
        return {
            'PF_menos_P2_g': round(p['PF']-p['P2'],4) if 'PF' in p and 'P2' in p else None,
            'P5_menos_P1_g': round(p['P5']-p['P1'],4) if 'P5' in p and 'P1' in p else None,
        }

    def resultado(self):
        return {'tipo':'ENSAIO_AMBIENTAL_SEM_AQUECIMENTO','id':self.id,
                'inicio':self.inicio,'fim':self.fim,'status':self.status,
                'instrumento_externo':'Omron MC-245 (termometro corporal; faixa 32-42 C)',
                'interpretacao':'L/H nao sao temperaturas; leituras manuais nao sao simultaneas a telemetria',
                'aquecimento_permitido':False,'pesagens_g':dict(self.pesagens),
                'diferencas':self.diferencas(),'eventos':list(self.eventos),
                'pontos':[{k:v for k,v in p.items() if k!='_relogio'} for p in self.pontos]}

    def salvar(self, pasta=DEFAULT_OUTPUT):
        if not self.id:
            raise ValueError('Não existe ensaio para exportar.')
        destino = Path(pasta)
        destino.mkdir(parents=True,exist_ok=True)
        json_path = destino / f'ensaio_ambiental_{self.id}.json'
        csv_path = destino / f'ensaio_ambiental_{self.id}.csv'
        dados = self.resultado()
        json_path.write_text(json.dumps(dados,ensure_ascii=False,indent=2),encoding='utf-8')
        colunas = ['marco_s','tempo_arduino_s','instante_arduino','ntc_c','massa_g',
                   'indicacao_mc245','temp_mc245_c','status_mc245','instante_manual',
                   'atraso_registro_s','observacao']
        with csv_path.open('w',encoding='utf-8-sig',newline='') as f:
            escritor=csv.DictWriter(f,fieldnames=colunas,delimiter=';')
            escritor.writeheader()
            for p in self.pontos:
                mc=p['mc245'] or {}
                escritor.writerow({'marco_s':p['marco_s'], 'tempo_arduino_s':p['tempo_arduino_s'],
                      'instante_arduino':p['instante_arduino'],'ntc_c':p['ntc_c'],
                      'massa_g':p['massa_g'],'indicacao_mc245':mc.get('indicacao',''),
                      'temp_mc245_c':mc.get('temperatura_c',''),
                      'status_mc245':mc.get('situacao','PENDENTE'),
                      'instante_manual':p['instante_manual'] or '',
                      'atraso_registro_s':p['atraso_registro_s'] if p['atraso_registro_s'] is not None else '',
                      'observacao':p['observacao']})
        self.arquivos=(json_path,csv_path)
        return self.arquivos
