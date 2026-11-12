import unittest
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import cv2
import numpy as np
from roi_automatica import detectar_roi, ROIFalhou

class TesteROI(unittest.TestCase):
    def test_imagem_invalida(self):
        with self.assertRaises(ROIFalhou): detectar_roi(None)
    def test_sem_bandeja(self):
        imagem=np.zeros((1152,2048,3),dtype=np.uint8)
        with self.assertRaises(ROIFalhou): detectar_roi(imagem)
    def test_circulo_sintetico(self):
        img=np.full((1000,1600,3), (55,70,100),dtype=np.uint8)
        cv2.circle(img,(810,498),427,(224,224,224),5)
        cv2.circle(img,(810,498),415,(200,200,200),5)
        r=detectar_roi(img)
        self.assertLess(abs(r.cx-810),35)
        self.assertLess(abs(r.cy-498),35)
        self.assertLess(abs(r.raio_bandeja-427),40)
    def test_circunferencia_proxima_borda_inferior(self):
        img=np.full((900,1600,3),(55,70,100),dtype=np.uint8)
        # aro externo com trecho inferior ligeiramente fora do quadro
        cv2.circle(img,(810,570),360,(224,224,224),5)
        cv2.circle(img,(810,570),348,(190,190,190),4)
        r=detectar_roi(img)
        self.assertLess(abs(r.cx-810),40)
        self.assertLess(abs(r.cy-570),40)
        self.assertTrue(r.cy+r.raio < img.shape[0])

    def test_arquivo_real(self):
        arquivo=Path(__file__).parent/'fotos_teste'/'imagem_1(6).jpg'
        if not arquivo.exists(): self.skipTest('Foto nao disponibilizada no ambiente')
        im=cv2.imread(str(arquivo))
        r=detectar_roi(im)
        self.assertLess(abs(r.cx-1004),40)
        self.assertLess(abs(r.cy-620),40)
        self.assertGreater(r.raio,350)

if __name__=='__main__': unittest.main()
