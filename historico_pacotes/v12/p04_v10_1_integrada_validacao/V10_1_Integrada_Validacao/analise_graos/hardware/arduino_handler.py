"""Cliente serial V10.1. Nao confundir MEDICAO com umidade de graos."""
import math
import time
import serial


class ArduinoHandler:
    def __init__(self, port='COM4', baudrate=9600, timeout=0.5, serial_obj=None):
        self.port = port
        self.baudrate = baudrate
        self.serial = serial_obj if serial_obj is not None else serial.Serial(
            port=port, baudrate=baudrate, timeout=timeout
        )
        if serial_obj is None:
            time.sleep(2)  # abertura da COM reinicia normalmente o Arduino Uno

    def _escrever(self, comando):
        if not self.serial.is_open:
            raise ConnectionError('Porta serial fechada')
        self.serial.write((comando + '\n').encode('ascii'))
        self.serial.flush()
        print(f'[PC -> UNO] {comando}')

    def _ler(self):
        if not self.serial.is_open:
            raise ConnectionError('Porta serial fechada')
        linha = self.serial.readline().decode('utf-8', errors='replace').strip()
        if linha:
            print(f'[UNO -> PC] {linha}')
            self._validar_erro(linha)
        return linha

    @staticmethod
    def _validar_erro(linha):
        if linha.startswith('ERRO:') or linha == 'TARA:ERRO':
            raise RuntimeError(f'Falha do Arduino: {linha}')

    @staticmethod
    def _numero(texto, nome):
        numero = float(texto)
        if not math.isfinite(numero):
            raise ValueError(f'{nome} nao finito')
        return numero

    def _esperar(self, esperado, timeout=30):
        fim = time.monotonic() + timeout
        while time.monotonic() < fim:
            linha = self._ler()
            if linha == esperado:
                return linha
            if linha == 'FIM:1' or linha.startswith('FIM:'):
                raise RuntimeError(f'Fim inesperado ao aguardar {esperado}: {linha}')
        raise TimeoutError(f'Timeout aguardando {esperado}')

    def aguardar_pronto(self, timeout=20):
        self._esperar('READY:V10.1', timeout)

    def realizar_tara(self):
        self._escrever('TARA')
        self._esperar('TARA:OK', 30)

    def iniciar_processo(self):
        self._escrever('START')

    def aguardar_p1_e_comando_camera(self, timeout=60):
        fim = time.monotonic() + timeout
        p1 = None
        while time.monotonic() < fim:
            linha = self._ler()
            if linha.startswith('P1:'):
                p1 = self._numero(linha.split(':', 1)[1], 'P1')
                if p1 <= 0:
                    raise ValueError('P1 deve ser positivo')
            elif linha == 'CMD:ACTIVATE_CAMERA':
                if p1 is None:
                    raise RuntimeError('Camera acionada antes de receber P1')
                return p1
        raise TimeoutError('Tempo esgotado aguardando P1 e camera')

    def enviar_resultado_cnn(self, status):
        comandos = {
            'APROVADA': 'CAM:SUCESSO',
            'REPROVADA': 'CAM:RUIM',
            'INCONCLUSIVA': 'CAM:INCONCLUSIVA'
        }
        if status not in comandos:
            raise ValueError(f'Status CNN nao reconhecido: {status}')
        self._escrever(comandos[status])

    def aguardar_fim_reprovacao(self, status, timeout=30):
        esperado = {
            'REPROVADA': 'FIM:REPROVADA_CNN',
            'INCONCLUSIVA': 'FIM:INCONCLUSIVA_CNN'
        }[status]
        self._esperar(esperado, timeout)

    def obter_serie_pesagens(self, sample, timeout=360):
        fim = time.monotonic() + timeout
        tem_p2 = tem_pf = False
        while time.monotonic() < fim:
            linha = self._ler()
            if not linha:
                continue
            if linha == 'FIM:1':
                if not (tem_p2 and tem_pf and sample.medicoes):
                    raise RuntimeError('Ciclo terminou sem P2, PF ou serie de medicoes')
                sample.calcular_variacao_massa()
                return sample
            if linha.startswith('FIM:'):
                raise RuntimeError(f'Finalizacao inesperada: {linha}')
            if linha.startswith('P2:'):
                sample.peso_referencia = self._numero(linha.split(':', 1)[1], 'P2')
                if sample.peso_referencia <= 0:
                    raise ValueError('P2 invalido')
                tem_p2 = True
            elif linha.startswith('MEDICAO:'):
                partes = linha.split(':')
                if len(partes) != 4:
                    raise ValueError(f'MEDICAO malformada: {linha}')
                seg = int(partes[1])
                peso = self._numero(partes[2], 'peso')
                temp = None if partes[3] == 'ERRO' else self._numero(partes[3], 'temp_ntc')
                if seg < 0 or (sample.medicoes and seg <= sample.medicoes[-1]['tempo_s']):
                    raise ValueError('Tempo de medicoes invalido ou nao crescente')
                if not tem_p2:
                    raise RuntimeError('MEDICAO recebida antes de P2')
                sample.medicoes.append({'tempo_s': seg, 'peso_g': peso, 'temp_ntc_c': temp})
                sample.temperatura_local = temp
            elif linha.startswith('PF:'):
                sample.peso_final = self._numero(linha.split(':', 1)[1], 'PF')
                tem_pf = True
            elif linha.startswith('MOVIMENTO:SIMULADO:'):
                sample.movimento_simulado = True
            elif linha.startswith('MOVIMENTO:REAL:'):
                sample.movimento_simulado = False
            elif linha.startswith('AQUECEDOR:'):
                sample.aquecimento_realizado = False
        raise TimeoutError('Timeout aguardando P2, serie temporal, PF e FIM:1')

    def parar(self):
        if self.serial.is_open:
            self._escrever('STOP')

    def fechar(self):
        if self.serial.is_open:
            self.serial.close()
