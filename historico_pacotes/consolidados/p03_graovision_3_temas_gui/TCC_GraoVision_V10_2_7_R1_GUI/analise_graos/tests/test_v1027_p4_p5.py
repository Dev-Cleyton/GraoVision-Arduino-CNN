import csv
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from hardware.arduino_handler import ArduinoHandler
from infrastructure.csv_service import salvar_relatorio
from models.sample import Sample
from models.analysis_session import AnalysisSession

class SerialFake:
    is_open = True
    def __init__(self, lines):
        self.lines = list(lines)
        self.sent = []
    def readline(self):
        return ((self.lines.pop(0) if self.lines else '')+'\n').encode('ascii')
    def write(self, data):
        self.sent.append(data.decode('ascii').strip())
    def flush(self):
        pass

class TesteP4P5(unittest.TestCase):
    def linhas(self, extra=True):
        ev=['P2:18.5000','MEDICAO:0:18.5000:24', 'MEDICAO:120:18.4900:24', 'PF:18.4900']
        ev += [f'VETOR:P3:{i}:{1000+i}:0:18.4000' for i in range(1,13)]
        ev += ['P3:18.4000','DIAG:AGUARDA_AUTORIZACAO']
        if extra:
            ev += ['DIAG:AUTORIZADO']
            ev += [f'VETOR:P4:{i}:{2000+i}:14000:18.5100' for i in range(1,13)]
            ev += ['P4:18.5100']
            ev += [f'VETOR:P5:{i}:{3000+i}:0:18.4050' for i in range(1,13)]
            ev += ['P5:18.4050']
        else:
            ev += ['DIAG:CANCELADO']
        return ev+['FIM:1']

    def test_autorizacao_e_csv(self):
        fs=SerialFake(self.linhas())
        h=ArduinoHandler(serial_obj=fs)
        s=Sample();s.peso_inicial=18.4
        with patch('builtins.input',return_value='AUTORIZAR-P4-P5'):
            h.obter_serie_pesagens(s,timeout=2,interativo=True)
        self.assertIn('DIAG:AUTORIZAR',fs.sent)
        self.assertEqual(len(s.vetores_pesagem['P4']),12)
        self.assertEqual(len(s.vetores_pesagem['P5']),12)
        self.assertAlmostEqual(s.repetibilidade_topo_g,0.01)
        self.assertAlmostEqual(s.repetibilidade_centro_g,0.005)
        ss=AnalysisSession();ss.sample=s
        with tempfile.TemporaryDirectory() as d:
            salvar_relatorio(ss,d)
            j=json.loads((Path(d)/f'{s.id}_relatorio.json').read_text(encoding='utf-8'))
            self.assertEqual(j['P4_g'],18.51)
            self.assertEqual(j['P5_g'],18.405)
            with (Path(d)/f'{s.id}_vetores_pesagem.csv').open(encoding='utf-8-sig') as f:
                self.assertEqual(len(list(csv.DictReader(f,delimiter=';'))),36)

    def test_cancelamento_seguro(self):
        fs=SerialFake(self.linhas(False))
        s=Sample();s.peso_inicial=18.4
        with patch('builtins.input',return_value=''):
            ArduinoHandler(serial_obj=fs).obter_serie_pesagens(s,timeout=2,interativo=True)
        self.assertIn('DIAG:CANCELAR',fs.sent)
        self.assertEqual(s.diagnostico_extra,'CANCELADO')
        self.assertIsNone(s.peso_segunda_subida)

    def test_firmware_fluxo_e_portas(self):
        for p in (Path(__file__).resolve().parents[2]/'arduino').glob('*/*.ino'):
            c=p.read_text()
            self.assertIn('case SUBIDA_DIAG:',c)
            self.assertIn('case PESA_P4:',c)
            self.assertIn('case DESCIDA_DIAG:',c)
            self.assertIn('case PESA_P5:',c)
            self.assertIn('DIAG:AGUARDA_AUTORIZACAO',c)
            self.assertIn('const bool AQUECEDOR_PERMITIDO_APOS_INSPECAO = false;',c)

if __name__=='__main__': unittest.main()
