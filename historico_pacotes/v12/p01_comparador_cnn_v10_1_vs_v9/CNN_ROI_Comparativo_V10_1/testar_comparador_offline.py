# -*- coding: utf-8 -*-
"""Autotestes OFFLINE: sem placa, sem camera, sem chamada ao Roboflow."""
import tempfile
import unittest
from pathlib import Path
import numpy as np
from comparar_cnn_roi_v101_v9 import (
    geometria, preparar_variantes, filtrar_deteccoes, estatisticas, executar)

class VerificarComparacao(unittest.TestCase):
    def setUp(self):
        self.img=np.ones((100,200,3),dtype=np.uint8)*42
        self.geom,self.caixa=geometria(self.img,100,50,35)

    def test_mascara_preserva_roi_e_neutraliza_fora(self):
        v,visual=preparar_variantes(self.img,self.geom,self.caixa)
        img=v['V9_RECORTE_MASCARA']
        self.assertEqual(tuple(img[35,35]),(42,42,42))
        self.assertEqual(tuple(img[0,0]),(125,125,125))
        self.assertEqual(self.img.shape,visual.shape)

    def test_coordenadas_remapeadas_consistentes(self):
        x1,y1,_,_=self.caixa
        completo=[{'x':110,'y':52,'width':5,'height':5,'class':'bad','confidence':.8}]
        crop=[{'x':110-x1,'y':52-y1,'width':5,'height':5,'class':'bad','confidence':.8}]
        a,_=filtrar_deteccoes(completo,self.geom,(0,0))
        b,_=filtrar_deteccoes(crop,self.geom,(x1,y1))
        self.assertEqual(a,b)
        self.assertEqual(estatisticas(a)['decisao'],'REPROVADA_CNN')

    def test_sem_deteccoes_inconclusiva(self):
        self.assertEqual(estatisticas([])['decisao'],'INCONCLUSIVA_SEM_DETECCOES')

    def test_preparacao_offline_nao_exige_chave(self):
        import cv2
        with tempfile.TemporaryDirectory() as tmp:
            f=Path(tmp)/'entrada.jpg'
            cv2.imwrite(str(f),self.img)
            out=Path(tmp)/'saida'
            resposta=executar(str(f),str(out),(100,50,35),None,None,True,None)
            self.assertIsNone(resposta)
            self.assertTrue((out/'V9_RECORTE_MASCARA_entrada.jpg').exists())
            self.assertFalse((out/'COMPARACAO_FINAL.json').exists())

if __name__=='__main__':unittest.main(verbosity=2)
