import csv
import json
import tempfile
import unittest
from pathlib import Path
from hardware.arduino_handler import ArduinoHandler
from models.sample import Sample
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio

class FakeSerial:
    is_open=True
    def __init__(self, lines): self.lines=list(lines)
    def readline(self):
        return ((self.lines.pop(0) if self.lines else '') + '\n').encode('ascii')
    def write(self, data): pass
    def flush(self): pass

class TesteV1026(unittest.TestCase):
    def test_vetor_p1_e_p3_com_fim(self):
        eventos=[f'VETOR:P1:{i}:{1000+i}:0:{18.0+i*0.0001:.4f}' for i in range(1,13)]
        eventos+=['P1:18.0007','CMD:ACTIVATE_CAMERA']
        eventos += [f'VETOR:P2:{i}:{2000+i}:14000:18.1000' for i in range(1,13)]
        eventos += ['P2:18.1000','MEDICAO:0:18.1000:24.0',
                    'MEDICAO:120:18.0900:24.1']
        eventos += [f'VETOR:PF:{i}:{3000+i}:14000:18.0900' for i in range(1,13)]
        eventos += ['PF:18.0900']
        eventos += [f'VETOR:P3:{i}:{4000+i}:0:18.0100' for i in range(1,13)]
        eventos += ['P3:18.0100','FIM:1']
        handler=ArduinoHandler(serial_obj=FakeSerial(eventos))
        s=Sample();handler.amostra_metrologia=s
        s.peso_inicial=handler.aguardar_p1_e_comando_camera(timeout=1)
        handler.obter_serie_pesagens(s,timeout=1)
        self.assertEqual(len(s.vetores_pesagem['P1']),12)
        self.assertEqual(len(s.vetores_pesagem['P3']),12)
        self.assertAlmostEqual(s.erro_retorno_g,0.0093,places=4)
        self.assertEqual(s.estatisticas_pesagem['P2']['desvio_padrao_g'],0)
        self.assertAlmostEqual(s.perda_massa_g,0.01)
        sess=AnalysisSession();sess.sample=s
        with tempfile.TemporaryDirectory() as pasta:
            salvar_relatorio(sess,pasta)
            j=json.loads((Path(pasta)/f'{s.id}_relatorio.json').read_text(encoding='utf8'))
            self.assertEqual(j['P3_g'],18.01)
            with (Path(pasta)/f'{s.id}_vetores_pesagem.csv').open(encoding='utf-8-sig') as f:
                self.assertEqual(len(list(csv.DictReader(f,delimiter=';'))),48)

    def test_p3_falha_nao_forca_umidade(self):
        s=Sample();s.peso_inicial=18.2
        s.diagnostico_p3='ERRO_DIAGNOSTICO'
        s.calcular_estatisticas_pesagem()
        self.assertIsNone(s.erro_retorno_g)
        self.assertIsNone(s.umidade_final)

    def test_rele_bloqueado_e_homing_nao_inventado(self):
        for arquivo in (Path(__file__).resolve().parents[2]/'arduino').glob('*/*.ino'):
            codigo=arquivo.read_text(encoding='utf8')
            self.assertIn('const bool AQUECEDOR_PERMITIDO_APOS_INSPECAO = false;',codigo)
            self.assertIn('case PESA_P3:',codigo)
            self.assertIn('case ESPERA_P3:',codigo)
            self.assertIn('F("FIM:1")',codigo)

if __name__ == '__main__': unittest.main()
