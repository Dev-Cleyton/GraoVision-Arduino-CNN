"""Testes da reabertura de camera, sem dispositivo fisico conectado."""
import unittest
from unittest.mock import patch, MagicMock
import numpy as np
from hardware.camera_handler import CameraHandler
# O pacote Roboflow real nao e necessario para testar liberacao da webcam.
with patch.dict('sys.modules', {'roboflow': MagicMock()}):
    from core.sample_service import SampleService


class FakeCap:
    def __init__(self, aberto=True, frames=True):
        self.aberto = aberto
        self.frames = frames
        self.solto = False
    def isOpened(self):
        return self.aberto and not self.solto
    def release(self):
        self.solto = True
    def set(self, _propriedade, _valor):
        return True
    def get(self, propriedade):
        return 2560 if propriedade == 3 else 1440
    def read(self):
        return (True, np.zeros((2, 2, 3), dtype='uint8')) if self.frames else (False, None)


class TesteCameraMulticiclos(unittest.TestCase):
    def _montar(self, caps, **kwargs):
        with patch('hardware.camera_handler.cv2.VideoCapture', side_effect=caps) as criar, \
             patch('hardware.camera_handler.time.sleep'), \
             patch.object(CameraHandler, 'estabilizar_camera', autospec=True):
            c = CameraHandler(tentativas_dshow=3, tentativas_msmf=2,
                              pausa_entre_tentativas_s=0, **kwargs)
        return c, criar

    def test_dshow_abre_primeira_tentativa(self):
        cap = FakeCap()
        camera, criar = self._montar([cap])
        self.assertEqual(criar.call_count, 1)
        self.assertEqual(camera.backend_ativo, 'DirectShow')
        camera.liberar(silencioso=True)
        self.assertTrue(cap.solto)

    def test_falha_temporaria_dshow_reabre(self):
        falha, sucesso = FakeCap(aberto=False), FakeCap()
        camera, criar = self._montar([falha, sucesso])
        self.assertEqual(criar.call_count, 2)
        self.assertTrue(falha.solto)
        self.assertEqual(camera.backend_ativo, 'DirectShow')
        camera.liberar(silencioso=True)

    def test_fallback_msmf_apos_tres_falhas(self):
        caps = [FakeCap(aberto=False) for _ in range(3)] + [FakeCap()]
        camera, criar = self._montar(caps)
        self.assertEqual(criar.call_count, 4)
        self.assertEqual(camera.backend_ativo, 'MediaFoundation')
        self.assertTrue(all(c.solto for c in caps[:3]))
        camera.liberar(silencioso=True)

    def test_abre_mas_nao_fornece_frame(self):
        sem_frames, sucesso = FakeCap(frames=False), FakeCap()
        camera, criar = self._montar([sem_frames, sucesso])
        self.assertEqual(criar.call_count, 2)
        self.assertTrue(sem_frames.solto)
        camera.liberar(silencioso=True)

    def test_falha_total_libera_capturas_e_retorna_erro(self):
        caps = [FakeCap(aberto=False) for _ in range(5)]
        with patch('hardware.camera_handler.cv2.VideoCapture', side_effect=caps), \
             patch('hardware.camera_handler.time.sleep'):
            with self.assertRaisesRegex(RuntimeError, 'DSHOW/MSMF'):
                CameraHandler(pausa_entre_tentativas_s=0)
        self.assertTrue(all(c.solto for c in caps))

    def test_falha_warmup_libera_dispositivo(self):
        cap = FakeCap()
        with patch('hardware.camera_handler.cv2.VideoCapture', return_value=cap), \
             patch('hardware.camera_handler.time.sleep'), \
             patch.object(CameraHandler, 'estabilizar_camera', side_effect=TimeoutError('luz ruim')):
            with self.assertRaises(TimeoutError):
                CameraHandler(pausa_entre_tentativas_s=0)
        self.assertTrue(cap.solto)

    def test_sample_service_soltar_camera_e_zerar_referencia(self):
        camera = MagicMock()
        servico = SampleService(camera=camera, classifier=MagicMock())
        servico.liberar_recursos()
        self.assertIsNone(servico.camera)
        camera.liberar.assert_called_once()
        servico.liberar_recursos()
        camera.liberar.assert_called_once()


if __name__ == '__main__':
    unittest.main()
