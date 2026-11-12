# -*- coding: utf-8 -*-
"""Testes offline da integracao da ROI; Mock do Roboflow, sem chave API."""
import csv
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch

# Se roboflow nao estiver instalado, permita somente os testes com model fake.
try:
    import roboflow
except ImportError:
    sys.modules['roboflow'] = types.SimpleNamespace(Roboflow=object)

import cv2
import numpy as np
from core.roi_automatica import ResultadoROI, ROIFalhou
from core.grain_classifier import GrainClassifier
from models.sample import Sample
from infrastructure.csv_service import salvar_relatorio
from models.analysis_session import AnalysisSession
from core.analysis_service import AnalysisService
from hardware.arduino_handler import ArduinoHandler
from test_protocolo_v101 import SerialFalso


class FakeResultado:
    def __init__(self, dados): self.dados = dados
    def json(self): return {'predictions': self.dados}

class FakeModel:
    def __init__(self, dados): self.dados = dados; self.chamadas = []
    def predict(self, caminho, confidence, overlap):
        self.chamadas.append((caminho, confidence, overlap))
        return FakeResultado(self.dados)

class TesteROIIntegrada(unittest.TestCase):
    def criar_imagem(self, pasta):
        caminho = Path(pasta)/'imagem_1.jpg'
        assert cv2.imwrite(str(caminho), np.zeros((480,640,3), dtype=np.uint8))
        return caminho

    def test_roi_automatica_filtra_e_persiste_comparacao(self):
        with tempfile.TemporaryDirectory() as d:
            caminho = self.criar_imagem(d)
            m = FakeModel([
                {'x':110,'y':100,'width':15,'height':15,'class':'bad','confidence':0.8},
                {'x':90,'y':100,'width':15,'height':15,'class':'good','confidence':0.9},
                {'x':350,'y':300,'width':15,'height':15,'class':'bad','confidence':0.5},
            ])
            sample=Sample(); sample.imagens=[str(caminho)]
            detector=ResultadoROI(100,100,90,110,0.9,0.88)
            with patch('core.grain_classifier.detectar_roi', return_value=detector):
                GrainClassifier(model=m, usar_roi_automatica=True).classificar(sample)
            self.assertEqual(sample.total_graos,2)
            self.assertEqual(sample.graos_ruins,1)
            self.assertEqual(sample.percentual_ruins,50)
            self.assertEqual(sample.roi_status,'OK')
            self.assertEqual(sample.roi_registros[0]['roi']['cx'],100)
            self.assertEqual(len(m.chamadas),1)
            self.assertEqual(m.chamadas[0][0],str(caminho)) # foto ORIGINAL
            registro=json.loads((Path(d)/'imagem_1_roi_automatica.json').read_text(encoding='utf8'))
            self.assertEqual(registro['deteccoes_aceitas'],2)
            self.assertEqual(registro['comparacao_roi_fixa']['deteccoes_aceitas'],1)
            with (Path(d)/'imagem_1_deteccoes.csv').open(encoding='utf-8-sig') as f:
                linhas=list(csv.DictReader(f,delimiter=';'))
            self.assertEqual(linhas[0]['dentro_roi'],'SIM')
            self.assertEqual(linhas[2]['dentro_roi'],'NAO')

    def test_falha_roi_impede_inferencia_sem_fallback(self):
        with tempfile.TemporaryDirectory() as d:
            caminho = self.criar_imagem(d)
            sample=Sample(); sample.imagens=[str(caminho)]
            m=FakeModel([{'x':120,'y':110,'class':'bad'}])
            with patch('core.grain_classifier.detectar_roi',side_effect=ROIFalhou('aro nao encontrado')):
                with self.assertRaisesRegex(ROIFalhou,'aro nao encontrado'):
                    GrainClassifier(model=m,usar_roi_automatica=True).classificar(sample)
            self.assertEqual(sample.roi_status,'REVISAR')
            self.assertEqual(m.chamadas, [])
            registro=json.loads((Path(d)/'imagem_1_roi_automatica.json').read_text(encoding='utf8'))
            self.assertFalse(registro['fallback_fixo_usado'])
            self.assertEqual(registro['status'],'REVISAR')

    def test_modo_legado_necessita_selecao_explicitamente(self):
        with tempfile.TemporaryDirectory() as d:
            caminho=self.criar_imagem(d)
            sample=Sample(); sample.imagens=[str(caminho)]
            m=FakeModel([{'x':320,'y':240,'width':20,'height':20,'class':'good','confidence':0.8}])
            with patch('core.grain_classifier.detectar_roi',side_effect=AssertionError('Nao chamar')):
                GrainClassifier(model=m,usar_roi_automatica=False).classificar(sample)
            self.assertEqual(sample.total_graos,1)
            self.assertEqual(sample.roi_modo,'FIXA_LEGADA_COMPARACAO')

    def test_falha_roi_interrompe_arduino_sem_cam_ruim(self):
        class ServicoFalha:
            def analisar_amostra(self, sample):
                sample.roi_status='REVISAR'
                sample.roi_registros=[{'status':'REVISAR','motivo':'aro ausente'}]
                raise ROIFalhou('aro ausente')
            def liberar_recursos(self): pass
        falso = SerialFalso()
        sessao = AnalysisService(arduino=ArduinoHandler(serial_obj=falso),
                                 sample_service=ServicoFalha()).executar(interativo=False,salvar=False)
        self.assertEqual(sessao.sample.status_final,'ERRO_PROCESSO')
        self.assertEqual(falso.enviados,['TARA','START','STOP'])
        self.assertNotIn('CAM:RUIM',falso.enviados)
        self.assertNotIn('CAM:SUCESSO',falso.enviados)

    def test_relatorio_roi_registrada(self):
        s=AnalysisSession()
        s.sample.roi_modo='AUTOMATICA_BANDEJA_V2'
        s.sample.roi_status='OK'
        s.sample.roi_registros=[{'status':'OK','roi':{'cx':1254,'cy':838,'raio':508}}]
        s.finalizar_sessao()
        with tempfile.TemporaryDirectory() as d:
            j,_=salvar_relatorio(s,d)
            dados=json.loads(j.read_text(encoding='utf8'))
            self.assertEqual(dados['roi_registros'][0]['roi']['cy'],838)

if __name__ == '__main__': unittest.main()
