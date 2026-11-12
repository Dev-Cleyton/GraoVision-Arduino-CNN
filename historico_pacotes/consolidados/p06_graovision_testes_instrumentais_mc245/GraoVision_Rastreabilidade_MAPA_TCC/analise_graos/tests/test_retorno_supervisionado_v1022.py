"""Validacao OFFLINE do contrato de recuperacao.
Nao verifica movimento fisico, torque, homing ou protecoes mecanicas.
"""
from collections import deque
import json
import pathlib
import tempfile
import unittest
from unittest.mock import patch
from hardware.arduino_handler import ArduinoHandler, FalhaArduino
from models.sample import Sample
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio

RAIZ = pathlib.Path(__file__).resolve().parents[2]
REAL = RAIZ / 'arduino' / 'V10_2_5_Controle_Termico_Real' / 'V10_2_5_Controle_Termico_Real.ino'
SIM = RAIZ / 'arduino' / 'V10_2_5_Controle_Termico_Simulacao' / 'V10_2_5_Controle_Termico_Simulacao.ino'

class SerialRecuperacao:
    def __init__(self, retorno='sucesso'):
        self.is_open = True
        self.retorno = retorno
        self.enviados = []
        self.fila = deque(['RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL\n'])
    def readline(self):
        return self.fila.popleft().encode() if self.fila else b''
    def write(self, payload):
        comando=payload.decode().strip()
        self.enviados.append(comando)
        if comando == 'RETORNAR:SUPERVISIONADO':
            if self.retorno == 'sucesso':
                self.fila.extend([
                    'RECUPERACAO:RETORNO_INICIADO\n',
                    'SERVO:PWM_REATIVADO:RETORNO\n',
                    'PASSOS:DESCIDA:0:14000:14000\n',
                    'PASSOS:DESCIDA:14000:14000:0\n',
                    'RECUPERACAO:RETORNO_COMANDADO:X_PENDENTE_CONFIRMACAO\n',
                ])
            else:
                self.fila.append('ERRO:EEPROM_FALHA_NO_RETORNO\n')
        return len(payload)
    def flush(self): pass
    def close(self):self.is_open=False

class RecuperacaoV1022(unittest.TestCase):
    def test_erro_tipado_preserva_codigo(self):
        h=ArduinoHandler(serial_obj=SerialRecuperacao())
        with self.assertRaises(FalhaArduino) as cm:
            h._validar_erro('ERRO:PESO_MONITOR_INSTAVEL')
        self.assertEqual(cm.exception.codigo,'PESO_MONITOR_INSTAVEL')

    @patch('builtins.input', return_value='RETORNAR')
    def test_retorno_confirmado_e_auditoria(self, _):
        porta=SerialRecuperacao()
        h=ArduinoHandler(serial_obj=porta)
        s=Sample()
        self.assertTrue(h.retorno_apos_erro(s, presencial=True, timeout=1))
        self.assertEqual(porta.enviados,['RETORNAR:SUPERVISIONADO'])
        self.assertEqual(s.recuperacao_status,'RETORNO_COMANDADO_CONFERIR_X')
        self.assertEqual(s.movimentos[-1]['posicao_comandada'],0)
        self.assertIn('SERVO:PWM_REATIVADO:RETORNO',s.eventos_servo_pwm)

    @patch('builtins.input', return_value='n')
    def test_cancelamento_nao_move(self, _):
        porta=SerialRecuperacao()
        h=ArduinoHandler(serial_obj=porta)
        s=Sample()
        self.assertFalse(h.retorno_apos_erro(s, presencial=True))
        self.assertEqual(s.recuperacao_status, 'CANCELADA_PELO_OPERADOR')
        self.assertEqual(porta.enviados, [])

    def test_sem_operador_nao_move(self):
        porta=SerialRecuperacao()
        h=ArduinoHandler(serial_obj=porta)
        s=Sample()
        self.assertFalse(h.retorno_apos_erro(s,presencial=False))
        self.assertEqual(porta.enviados, [])
        self.assertEqual(s.recuperacao_status,'BLOQUEADA_SEM_OPERADOR')

    @patch('builtins.input', return_value='RETORNAR')
    def test_falha_durante_retorno_bloqueia(self, _):
        porta=SerialRecuperacao('falha')
        h=ArduinoHandler(serial_obj=porta)
        s=Sample()
        self.assertFalse(h.retorno_apos_erro(s, presencial=True, timeout=1))
        self.assertEqual(s.recuperacao_status,'FALHA_DURANTE_RETORNO')
        self.assertIn('ERRO:EEPROM_FALHA_NO_RETORNO',s.eventos_recuperacao)

    def test_relatorio_preserva_erro_e_retorno(self):
        sess=AnalysisSession()
        sess.sample.codigo_erro_arduino='PESO_MONITOR_INSTAVEL'
        sess.sample.recuperacao_status='RETORNO_COMANDADO_CONFERIR_X'
        sess.sample.eventos_recuperacao=['RECUPERACAO:RETORNO_INICIADO']
        with tempfile.TemporaryDirectory() as td:
            salvar_relatorio(sess,td)
            dado=json.loads((pathlib.Path(td)/f'{sess.sample.id}_relatorio.json').read_text(encoding='utf8'))
        self.assertEqual(dado['codigo_erro_arduino'],'PESO_MONITOR_INSTAVEL')
        self.assertEqual(dado['recuperacao_status'],'RETORNO_COMANDADO_CONFERIR_X')

    def test_stop_durante_pesagem_nunca_libera_retorno(self):
        fonte = REAL.read_text(encoding='utf8')
        trecho = fonte.split('void falharRecuperavel(const __FlashStringHelper *motivo) {', 1)[1].split('void trocarEstado', 1)[0]
        self.assertIn('if (paradaSolicitada || estado == ESTADO_FALHA) return;', trecho)
        self.assertIn('!motorEmMovimento', trecho)

    def test_firmware_retorno_somente_confirmado(self):
        s=REAL.read_text(encoding='utf8')
        self.assertIn('RETORNAR:SUPERVISIONADO',s)
        self.assertIn('retornoSupervisionadoDisponivel',s)
        self.assertIn('!motorEmMovimento',s)
        self.assertIn('falharRecuperavel(F("PESO_MONITOR_INSTAVEL"))',s)
        self.assertIn('RECUPERACAO:RETORNO_COMANDADO:X_PENDENTE_CONFIRMACAO',s)
        self.assertIn('recuperacaoPendente = true;',s)
        self.assertIn('SEGURANCA:DRIVER_MANTIDO:POSICAO_ACIMA_DE_X',s)
        self.assertIn('PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO = false;',SIM.read_text(encoding='utf8'))
        trecho=s.split('while (posicaoPassos != destino) {',1)[1].split('// Grava a posicao final JA COM O MOTOR PARADO;',1)[0]
        self.assertNotIn('persistir(',trecho)
        self.assertIn('AQUECEDOR:BLOQUEADO_V10_2_5',s)

if __name__=='__main__':unittest.main()
