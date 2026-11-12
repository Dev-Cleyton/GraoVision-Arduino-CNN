"""Regressoes de protocolo e seguranca: nao substituem a placa fisica."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from hardware.arduino_handler import ArduinoHandler
from models.sample import Sample
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio

R = Path(__file__).resolve().parents[2]
UNO = R/'arduino/V10_2_5_Controle_Termico_Real/V10_2_5_Controle_Termico_Real.ino'
SIM = R/'arduino/V10_2_5_Controle_Termico_Simulacao/V10_2_5_Controle_Termico_Simulacao.ino'

class FakeSerial:
    is_open=True
    def __init__(self, lines):
        self.lines=list(lines); self.writes=[]
    def readline(self):
        return ((self.lines.pop(0) if self.lines else '') + '\n').encode('ascii')
    def write(self, data): self.writes.append(data)
    def flush(self): pass

class TesteV1025(unittest.TestCase):
    def test_gate_de_aquecimento_fechado_em_ambos(self):
        for p in (UNO,SIM):
            s=p.read_text(encoding='utf8')
            self.assertIn('const bool AQUECEDOR_PERMITIDO_APOS_INSPECAO = false;',s)
            self.assertIn('aquecimentoArmado && AQUECEDOR_PERMITIDO_APOS_INSPECAO',s)
            self.assertIn('AQUECEDOR:NAO_AUTORIZADO_NESTA_COMPILACAO',s)
            self.assertIn('TIMEOUT_HEARTBEAT_PC_MS = 5000UL;',s)
    def test_pf_novo_apos_monitoramento(self):
        s=UNO.read_text(encoding='utf8')
        trecho=s.split('case MONITORANDO: {',1)[1].split('case AGUARDAR_RESFRIAMENTO',1)[0]
        self.assertIn('millis() - inicioSerie >= TEMPO_MONITOR_MS',trecho)
        self.assertIn('desligarAquecedor(F("FIM_MONITORAMENTO"))',trecho)
        self.assertIn('pesarVetor(F("PF"), pf)',trecho)
        self.assertIn('emitirPeso(F("PF"), pf)',trecho)
        self.assertNotIn('pf = leitura;',trecho)
    def test_resfriamento_sem_retorno_automatico(self):
        s=UNO.read_text(encoding='utf8')
        self.assertIn('CONFIRMO:RESFRIAMENTO',s)
        self.assertIn('if (confirmacaoResfriamentoOperador)',s)
        self.assertIn('!aquecimentoFoiComandado',s)
    def test_registra_on_off_no_python(self):
        sample=Sample()
        f=ArduinoHandler.registrar_evento_termico
        f(sample,'AQUECEDOR:ON_COMANDADO:0')
        f(sample,'AQUECEDOR:OFF:TEMPO_120S')
        f(sample,'AQUECEDOR:TEMPO_COMANDO_ON_MS:120006')
        f(sample,'RESFRIAMENTO:15:36.1')
        self.assertTrue(sample.aquecimento_comandado)
        self.assertEqual(sample.tempo_comando_on_ms,120006)
        self.assertEqual(sample.motivo_desligamento_aquecedor,'TEMPO_120S')
        self.assertEqual(sample.medicoes_resfriamento,[{'tempo_s':15,'temp_ntc_c':36.1}])
        sess=AnalysisSession(); sess.sample=sample
        with tempfile.TemporaryDirectory() as tmp:
            arq,_=salvar_relatorio(sess,tmp)
            d=json.loads(arq.read_text(encoding='utf8'))
            self.assertEqual(d['tempo_comando_on_ms'],120006)
            self.assertEqual(len(d['eventos_aquecedor']),3)
            self.assertTrue((Path(tmp)/f'{sample.id}_eventos_termicos.csv').exists())
    def test_armamento_recusado_pelo_arduino(self):
        fake=FakeSerial(['AQUECEDOR:NAO_AUTORIZADO_NESTA_COMPILACAO'])
        h=ArduinoHandler(serial_obj=fake)
        with self.assertRaisesRegex(RuntimeError, 'BLOQUEADO'):
            h.armar_aquecimento_apos_inspecao()
        self.assertIn(b'ARMAR:AQUECIMENTO\n',fake.writes)

if __name__=='__main__': unittest.main()
