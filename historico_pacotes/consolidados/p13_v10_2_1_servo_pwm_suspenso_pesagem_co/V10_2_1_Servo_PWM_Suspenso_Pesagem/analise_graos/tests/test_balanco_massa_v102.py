"""Testes de calculos sem hardware ou ferramentas externas."""
import csv
import json
import tempfile
import unittest
from pathlib import Path
from core.balanco_massa import calcular_balanco
from models.sample import Sample
from models.analysis_session import AnalysisSession
from infrastructure.csv_service import salvar_relatorio


class TestarBalancoMassaV102(unittest.TestCase):
    def test_diminuiu(self):
        r=calcular_balanco(10.8414, 10.8994, 10.4994)
        self.assertAlmostEqual(r['offset_posicao_P2_menos_P1_g'], 0.058)
        self.assertAlmostEqual(r['PF_corrigido_referencia_P1_g'], 10.4414)
        self.assertAlmostEqual(r['perda_massa_aparente_g'], 0.4)
        self.assertEqual(r['tendencia_leitura'], 'DIMINUIU')

    def test_aumentou_sem_aquecimento(self):
        r=calcular_balanco(10.8414,10.8994,10.9531)
        self.assertAlmostEqual(r['perda_massa_aparente_g'], -0.0537)
        self.assertAlmostEqual(r['PF_corrigido_referencia_P1_g'],10.8951)
        self.assertEqual(r['tendencia_leitura'],'AUMENTOU')

    def test_offset_negativo(self):
        r=calcular_balanco(20.6875,20.6603,20.6732)
        self.assertAlmostEqual(r['offset_posicao_P2_menos_P1_g'],-0.0272)
        self.assertAlmostEqual(r['PF_corrigido_referencia_P1_g'],20.7004)
        self.assertAlmostEqual(r['perda_massa_aparente_g'],-0.0129)

    def test_identidade_algebrica(self):
        for p1,p2,pf in [(10,10.1,9),(15,14.9,14.7),(20,21,22)]:
            r=calcular_balanco(p1,p2,pf)
            self.assertAlmostEqual(p1-r['PF_corrigido_referencia_P1_g'], p2-pf)

    def test_entrada_invalida(self):
        for trio in [(0,1,1),(1,0,1),(1,1,float('nan')),(1,1,float('inf')),(None,1,1),(1,1,-1)]:
            with self.assertRaises(ValueError):calcular_balanco(*trio)

    def test_sample_preserva_pesos(self):
        s=Sample();s.peso_inicial=10.8414;s.peso_referencia=10.8994;s.peso_final=10.9531
        s.calcular_variacao_massa()
        self.assertAlmostEqual(s.peso_inicial,10.8414)
        self.assertAlmostEqual(s.peso_referencia,10.8994)
        self.assertAlmostEqual(s.peso_final,10.9531)
        self.assertAlmostEqual(s.perda_massa_g,-0.0537)
        self.assertIsNone(s.umidade_final)
        self.assertFalse(s.aquecimento_realizado)

    def test_reprova_sem_p2_nao_cria_estimativa(self):
        s=Sample();s.peso_inicial=12.3;s.calcular_variacao_massa()
        self.assertIsNone(s.perda_massa_g)
        self.assertIsNone(s.peso_final_corrigido_g)

    def test_saida_json_csv(self):
        sessao=AnalysisSession(); s=sessao.sample
        s.peso_inicial=10.8414;s.peso_referencia=10.8994;s.peso_final=10.9531
        s.calcular_variacao_massa();sessao.finalizar_sessao()
        with tempfile.TemporaryDirectory() as d:
            arq_json,arq_csv=salvar_relatorio(sessao,d)
            dado=json.loads(arq_json.read_text(encoding='utf-8'))
            self.assertAlmostEqual(dado['PF_corrigido_referencia_P1_g'],10.8951)
            self.assertIsNone(dado['umidade_graos_final_percentual'])
            arq_balanco=Path(d)/(s.id+'_balanco_massa.csv')
            with arq_balanco.open(encoding='utf-8-sig',newline='') as arq:
                linhas=list(csv.DictReader(arq,delimiter=';'))
            self.assertEqual(len(linhas),1)
            self.assertEqual(linhas[0]['tendencia_leitura_massa'],'AUMENTOU')

if __name__=='__main__':unittest.main()
