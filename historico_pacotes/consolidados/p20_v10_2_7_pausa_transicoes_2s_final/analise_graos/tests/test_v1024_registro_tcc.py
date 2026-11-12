"""Verifica que a V10.2.4 documenta o ensaio sem simular aquecimento."""
import json
import tempfile
import unittest
from pathlib import Path
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio

RAIZ = Path(__file__).resolve().parents[2]

class TesteRegistroTCCV1024(unittest.TestCase):
    def test_registro_massas_e_temperatura_camara(self):
        sessao = AnalysisSession()
        s = sessao.sample
        s.peso_inicial = 10.8414
        s.peso_referencia = 10.8994
        s.peso_final = 10.4994
        s.temperatura_local = 33.4
        s.temperatura_camara = 33.4
        s.medicoes = [{'tempo_s': 0, 'peso_g': 10.8994, 'temp_ntc_c': 32.2},
                     {'tempo_s': 120, 'peso_g': 10.4994, 'temp_ntc_c': 33.4}]
        s.calcular_variacao_massa()
        sessao.finalizar_sessao()
        with tempfile.TemporaryDirectory() as tmp:
            arq_json, _ = salvar_relatorio(sessao, tmp)
            dado = json.loads(arq_json.read_text(encoding='utf-8'))
            resumo = (Path(tmp) / f'{s.id}_resultado_tcc.md').read_text(encoding='utf-8')
            self.assertEqual(dado['ntc_localizacao_atual'], 'CAMARA_A2')
            self.assertEqual(dado['temperatura_ntc_camara_C'], 33.4)
            self.assertFalse(dado['aquecedor_habilitado_por_firmware'])
            self.assertAlmostEqual(dado['perda_massa_g'], .4)
            self.assertIn('Offset P2-P1', resumo)
            self.assertIn('33.40', resumo)
            self.assertIn('Rele D7 recebeu comando ON | NAO', resumo)
            self.assertIn('NAO valida desidratacao', resumo)

    def test_erro_sem_pf_nao_inventa_perda(self):
        sessao = AnalysisSession()
        s = sessao.sample
        s.peso_inicial = 16.6949
        s.peso_referencia = 16.7721
        s.calcular_variacao_massa()
        sessao.finalizar_sessao()
        with tempfile.TemporaryDirectory() as tmp:
            arq_json, _ = salvar_relatorio(sessao, tmp)
            dado = json.loads(arq_json.read_text(encoding='utf-8'))
            self.assertIsNone(dado['PF_g'])
            self.assertIsNone(dado['perda_massa_g'])
            self.assertAlmostEqual(dado['delta_posicionamento_P2_menos_P1_g'], .0772)

    def test_firmware_mantem_aquecedor_off(self):
        for arq in (RAIZ / 'arduino').glob('*/*.ino'):
            codigo = arq.read_text(encoding='utf-8')
            self.assertIn('NTC:LOCALIZACAO:CAMARA:A2', codigo)
            self.assertIn('const bool AQUECEDOR_PERMITIDO_APOS_INSPECAO = false;', codigo)
            self.assertIn('digitalWrite(RELE_AQUECEDOR, HIGH)', codigo)

if __name__ == '__main__':
    unittest.main()
