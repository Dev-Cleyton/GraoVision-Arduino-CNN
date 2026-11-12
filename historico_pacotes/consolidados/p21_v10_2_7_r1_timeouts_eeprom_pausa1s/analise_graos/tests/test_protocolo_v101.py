"""Testes unitarios OFFLINE. Nao conecta Arduino nem Roboflow."""
import csv
import json
import sys
import tempfile
import types
import unittest
from collections import deque
from pathlib import Path
from unittest.mock import patch

# Em computadores sem dependencias instaladas, mock para testes de logica.
try:
    import serial
except ImportError:
    serial_stub = types.ModuleType('serial')
    serial_stub.Serial = lambda *args, **kwargs: None
    sys.modules['serial'] = serial_stub
try:
    import roboflow
except ImportError:
    rf_stub = types.ModuleType('roboflow')
    rf_stub.Roboflow = object
    sys.modules['roboflow'] = rf_stub

from hardware.arduino_handler import ArduinoHandler
from models.sample import Sample
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio
from core.analysis_service import AnalysisService
from aplicar_umidade_validada import aplicar


class SerialFalso:
    def __init__(self, resultado='APROVADA', serie_valida=True):
        self.is_open = True
        self.resultado = resultado
        self.serie_valida = serie_valida
        self.fila = deque(['V10.1:VALIDACAO_SEM_AQUECIMENTO\n', 'READY:V10.1\n'])
        self.enviados = []

    def write(self, b):
        cmd = b.decode('ascii').strip()
        self.enviados.append(cmd)
        if cmd == 'CENTRALIZAR':
            self.fila.extend(['MOVIMENTO:SIMULADO:SERVO:90\n', 'SERVO:CENTRO:OK\n'])
        elif cmd == 'TARA':
            self.fila.append('TARA:OK\n')
        elif cmd == 'START':
            self.fila.extend(['P1:35.9000\n', 'MOVIMENTO:SIMULADO:SERVO:0\n', 'CMD:ACTIVATE_CAMERA\n'])
        elif cmd == 'CAM:RUIM':
            self.fila.extend(['MOVIMENTO:SIMULADO:SERVO:90\n', 'FIM:REPROVADA_CNN\n', 'READY:V10.1\n'])
        elif cmd == 'CAM:INCONCLUSIVA':
            self.fila.extend(['FIM:INCONCLUSIVA_CNN\n', 'READY:V10.1\n'])
        elif cmd == 'CAM:SUCESSO':
            self.fila.extend(['MOVIMENTO:SIMULADO:SUBIDA\n', 'P2:35.8900\n',
                              'AQUECEDOR:BLOQUEADO_V10_2_5\n',
                              'MEDICAO:0:35.8900:27.30\n',
                              'MEDICAO:30:35.8800:27.31\n'])
            if self.serie_valida:
                self.fila.extend(['MEDICAO:60:35.8700:27.32\n',
                                  'MEDICAO:90:35.8700:27.33\n',
                                  'MEDICAO:120:35.8600:27.34\n',
                                  'PF:35.8600\n'])
            self.fila.extend(['FIM:1\n', 'READY:V10.1\n'])
        elif cmd == 'STOP':
            self.fila.append('ERRO:STOP_OPERADOR\n')
        return len(b)

    def readline(self):
        return self.fila.popleft().encode('utf8') if self.fila else b''

    def flush(self):
        pass

    def close(self):
        self.is_open = False


class ClassificadorFalso:
    def __init__(self, status):
        self.status = status

    def analisar_amostra(self, sample):
        sample.imagens = ['imagem_simulada.jpg']
        sample.total_graos = 100 if self.status != 'INCONCLUSIVA' else 0
        sample.graos_ruins = 32 if self.status == 'REPROVADA' else 12
        sample.percentual_ruins = 32 if self.status == 'REPROVADA' else 12
        sample.classes_detectadas = {'bad': sample.graos_ruins}
        sample.status_cnn = self.status
        sample.motivo_cnn = '32% bad, maior que 25%' if self.status == 'REPROVADA' else 'teste de bancada'
        sample.cnn_aprovada = self.status == 'APROVADA'
        return sample

    def liberar_recursos(self):
        pass


class TesteV101(unittest.TestCase):
    def executar(self, status, serie_valida=True):
        falso = SerialFalso(resultado=status, serie_valida=serie_valida)
        arduino = ArduinoHandler(serial_obj=falso)
        servico = AnalysisService(arduino=arduino, sample_service=ClassificadorFalso(status))
        sessao = servico.executar(interativo=False, salvar=False)
        return sessao, falso

    def test_aprovada_cnn_sem_umidade_validada(self):
        sessao, fake = self.executar('APROVADA')
        s = sessao.sample
        self.assertEqual(s.status_final, 'PENDENTE_VALIDACAO_UMIDADE')
        self.assertAlmostEqual(s.peso_inicial, 35.9)
        self.assertAlmostEqual(s.peso_referencia, 35.89)
        self.assertAlmostEqual(s.peso_final, 35.86)
        self.assertAlmostEqual(s.perda_massa_percentual, 100*(35.89-35.86)/35.89)
        self.assertEqual(len(s.medicoes), 5)
        self.assertIsNone(s.umidade_final)
        self.assertFalse(s.aprovado)
        self.assertFalse(s.aquecimento_realizado)
        self.assertEqual(fake.enviados, ['CENTRALIZAR', 'TARA', 'START', 'CAM:SUCESSO'])
        self.assertFalse(sessao.possui_erro)

    def test_reprova_cnn_sem_secagem(self):
        sessao, fake = self.executar('REPROVADA')
        self.assertEqual(sessao.sample.status_final, 'REPROVADA_CNN')
        self.assertEqual(fake.enviados, ['CENTRALIZAR', 'TARA', 'START', 'CAM:RUIM'])
        self.assertIsNone(sessao.sample.peso_referencia)
        self.assertFalse(sessao.possui_erro)

    def test_inconclusiva_nao_reprova_por_defeito_inventado(self):
        sessao, fake = self.executar('INCONCLUSIVA')
        self.assertEqual(sessao.sample.status_final, 'INCONCLUSIVA_CNN')
        self.assertEqual(fake.enviados, ['CENTRALIZAR', 'TARA', 'START', 'CAM:INCONCLUSIVA'])
        self.assertFalse(sessao.possui_erro)

    def test_fim_sem_pf_falha(self):
        sessao, fake = self.executar('APROVADA', serie_valida=False)
        self.assertTrue(sessao.possui_erro)
        self.assertEqual(sessao.sample.status_final, 'ERRO_PROCESSO')
        self.assertIn('STOP', fake.enviados)

    def test_criterio_umidade_validada(self):
        sample = Sample()
        sample.cnn_aprovada = True
        sample.avaliar_umidade_validada(16.2, 14, 'Metodo de laboratorio validado')
        self.assertEqual(sample.status_final, 'REPROVADA_UMIDADE')
        sample.avaliar_umidade_validada(13, 14, 'Metodo de laboratorio validado')
        self.assertEqual(sample.status_final, 'APROVADA')
        self.assertTrue(sample.aprovado)
        with self.assertRaises(ValueError):
            sample.avaliar_umidade_validada(13, 14, '')

    def test_umidade_lab_complementar(self):
        sessao, _ = self.executar('APROVADA')
        with tempfile.TemporaryDirectory() as d:
            j, _ = salvar_relatorio(sessao, d)
            novo = aplicar(j, 16.2, 14.0, 'Metodo gravimetrico validado', 'LAUDO-100')
            dct = json.loads(novo.read_text(encoding='utf8'))
            self.assertEqual(dct['status'], 'REPROVADA_UMIDADE')
            self.assertIn('16.20%', dct['motivo'])
            self.assertNotEqual(j, novo)

    def test_relatorio_integridade(self):
        sessao, _ = self.executar('REPROVADA')
        with tempfile.TemporaryDirectory() as d:
            j, c = salvar_relatorio(sessao, d)
            dados = json.loads(Path(j).read_text(encoding='utf8'))
            self.assertEqual(dados['status'], 'REPROVADA_CNN')
            self.assertIsNone(dados['umidade_graos_final_percentual'])
            with Path(c).open(encoding='utf-8-sig') as f:
                linhas = list(csv.DictReader(f, delimiter=';'))
            self.assertEqual(len(linhas), 0)


if __name__ == '__main__':
    unittest.main()
