"""Testes especificos R2: recuperacao, centro, telemetria de passos, JSON."""
from collections import deque
from unittest.mock import patch
import tempfile
import json
import unittest

from hardware.arduino_handler import ArduinoHandler
from models.sample import Sample
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio


class SerialMock:
    is_open = True
    def __init__(self, linhas=()):
        self.fila = deque((linha + '\n').encode() for linha in linhas)
        self.enviados = []
    def write(self, payload):
        cmd = payload.decode('ascii').strip()
        self.enviados.append(cmd)
        if cmd == 'RECUPERAR:X':
            self.fila.extend([b'RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL\n', b'READY:V10.1\n'])
        if cmd == 'CENTRALIZAR':
            self.fila.extend([b'MOVIMENTO:REAL:SERVO\n', b'SERVO:CENTRO:OK\n'])
        return len(payload)
    def readline(self): return self.fila.popleft() if self.fila else b''
    def flush(self): pass
    def close(self): self.is_open = False


class TesteR2(unittest.TestCase):
    def test_recuperacao_so_com_confirmacao_fisica(self):
        f = SerialMock(['RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR'])
        with patch('builtins.input', return_value='CONFIRMO-X'):
            ArduinoHandler(serial_obj=f).aguardar_pronto(recuperar_interativamente=True)
        self.assertEqual(f.enviados, ['RECUPERAR:X'])

    def test_recuperacao_bloqueada_sem_confirmacao(self):
        f = SerialMock(['RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR'])
        with patch('builtins.input', return_value='NAO'):
            with self.assertRaises(RuntimeError):
                ArduinoHandler(serial_obj=f).aguardar_pronto(recuperar_interativamente=True)
        self.assertEqual(f.enviados, [])

    def test_centralizacao_antes_da_tara(self):
        f = SerialMock()
        ArduinoHandler(serial_obj=f).centralizar_antes_tara()
        self.assertEqual(f.enviados, ['CENTRALIZAR'])

    def test_progresso_e_relatorio(self):
        f = SerialMock([
            'PASSOS:SUBIDA:0:14000:0',
            'PASSOS:SUBIDA:500:14000:500',
            'PASSOS:SUBIDA:14000:14000:14000',
            'P2:11.1000', 'MEDICAO:0:11.1000:25.0',
            'PASSOS:DESCIDA:0:14000:14000',
            'PASSOS:DESCIDA:14000:14000:0',
            'MEDICAO:120:11.0900:25.1', 'PF:11.0900', 'FIM:1'
        ])
        sample = Sample()
        sample.peso_inicial=11.10
        ArduinoHandler(serial_obj=f).obter_serie_pesagens(sample)
        self.assertEqual(len(sample.movimentos),5)
        self.assertEqual(sample.movimentos[1]['passos_executados'],500)
        self.assertEqual(sample.movimentos[-1]['posicao_comandada'],0)
        self.assertFalse(sample.movimento_simulado)
        sessao=AnalysisSession()
        sessao.sample=sample
        sessao.finalizar_sessao()
        with tempfile.TemporaryDirectory() as temp:
            j, _ = salvar_relatorio(sessao, temp)
            d=json.loads(j.read_text(encoding='utf-8'))
            self.assertEqual(len(d['movimentos_motor']),5)
            mov=j.with_name(j.name.replace('_relatorio.json','_movimentos.csv'))
            self.assertTrue(mov.is_file())

if __name__=='__main__': unittest.main()
