"""Teste do coletor do ensaio instrumental sem depender de serial ou GUI."""
from interface.ensaios_instrumentais import EnsaioInstrumental, interpretar_mc245
import csv
import json
import pytest


def test_mc245_faixa_operacao_e_indicacao_L():
    assert interpretar_mc245('L')['temperatura_c'] is None
    assert interpretar_mc245('low')['situacao']=='FORA_FAIXA_INFERIOR'
    assert interpretar_mc245('H')['temperatura_c'] is None
    assert interpretar_mc245('36,5')['temperatura_c']==36.5
    for entrada in ('25,7','31.9','43','texto',''):
        with pytest.raises(ValueError):interpretar_mc245(entrada)


def test_telemetria_e_registro_manual_exporta_csv_json(tmp_path):
    horas=[0.0]
    def relogio(): return horas[0]
    reg=EnsaioInstrumental(agora=lambda:'2026-11-11T11:00:00-03:00',relogio=relogio)
    reg.iniciar()
    for t,temper,peso in [(0,25.0,12.50),(30,25.2,12.52),(60,25.1,12.51),
                         (91,25.0,12.49),(121,24.9,12.50)]:
        horas[0]=t
        assert reg.receber(f'MEDICAO:{t}:{peso}:{temper}')[0]=='ponto'
    assert len(reg.pontos)==5
    assert [x['marco_s'] for x in reg.pontos]==[0,30,60,90,120]
    assert reg.receber('MEDICAO:120:12.5:26') is None
    reg.receber('P1:12.5000');reg.receber('P2:12.5200');reg.receber('PF:12.4900');reg.receber('P5:12.4800')
    horas[0]=123.0
    ponto=reg.registrar_mc245(120,'L','temperatura ambiente')
    assert ponto['mc245']['temperatura_c'] is None
    assert ponto['atraso_registro_s']==2.0
    with pytest.raises(ValueError):reg.registrar_mc245(10,'L')
    assert reg.diferencas()['PF_menos_P2_g']==-0.03
    reg.finalizar('CONCLUIDO_SEM_CALOR')
    jp,cp=reg.salvar(tmp_path)
    data=json.loads(jp.read_text(encoding='utf-8'))
    assert data['aquecimento_permitido'] is False
    assert len(data['pontos'])==5
    assert data['pontos'][-1]['mc245']['indicacao']=='L'
    with cp.open(encoding='utf-8-sig') as f:
        linhas=list(csv.DictReader(f,delimiter=';'))
    assert len(linhas)==5
    assert linhas[-1]['status_mc245']=='FORA_FAIXA_INFERIOR'


def test_aquecimento_inesperado_emite_alerta():
    reg=EnsaioInstrumental()
    reg.iniciar()
    assert reg.receber('AQUECEDOR:ON_COMANDADO:120')[0]=='alerta'
    reg.finalizar()
    assert reg.status=='ALERTA_AQUECEDOR'


def test_modulo_nao_transmite_comandos_motor_ou_calor():
    from pathlib import Path
    texto=Path(__file__).parents[1].joinpath('interface','testes_instrumentais.py').read_text(encoding='utf-8')
    assert 'start_cycle(instrumental=True)' in texto
    assert "ARMAR:AQUECIMENTO" not in texto
    assert "TESTE:SUBIR" not in texto
