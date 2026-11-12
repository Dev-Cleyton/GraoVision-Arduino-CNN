"""Webcam V10.1B V2: reabertura robusta entre ciclos (Windows / DirectShow)."""
import cv2
import os
import time
import numpy as np


class CameraHandler:
    def __init__(
        self,
        camera_index=0,
        largura=2560,
        altura=1440,
        frames_estabilizacao=30,
        luminosidade_minima=65.94,
        tentativas_dshow=3,
        tentativas_msmf=2,
        pausa_entre_tentativas_s=1.5,
        on_frame=None,
    ):
        self.on_frame = on_frame
        self._last_frame_emit = 0.0
        self.camera_index = camera_index
        self.largura = largura
        self.altura = altura
        self.frames_estabilizacao = frames_estabilizacao
        self.luminosidade_minima = luminosidade_minima
        self.cap = None
        self.backend_ativo = None
        self._abrir_camera(
            tentativas_dshow=tentativas_dshow,
            tentativas_msmf=tentativas_msmf,
            pausa_entre_tentativas_s=pausa_entre_tentativas_s,
        )

    def _publicar_preview(self, frame):
        """Miniatura JPEG limitada a 5 fps; nao envia imagem pela rede."""
        if self.on_frame is None or frame is None:
            return
        agora = time.monotonic()
        if agora - self._last_frame_emit < 0.20:
            return
        self._last_frame_emit = agora
        try:
            h, w = frame.shape[:2]
            miniatura = cv2.resize(frame, (min(720, w), max(1, int(h * min(720, w) / w))))
            ok, encoded = cv2.imencode('.jpg', miniatura, [cv2.IMWRITE_JPEG_QUALITY, 70])
            if ok:
                self.on_frame(encoded.tobytes())
        except Exception:
            pass  # A interface jamais pode interromper a aquisicao cientifica.

    def _abrir_camera(self, tentativas_dshow, tentativas_msmf, pausa_entre_tentativas_s):
        # DSHOW era o backend da V10.1B. MSMF e uma alternativa quando
        # o Windows recusa reabrir DSHOW; nao e uma troca permanente.
        estrategias = (
            ('DirectShow', cv2.CAP_DSHOW, max(0, int(tentativas_dshow))),
            ('MediaFoundation', cv2.CAP_MSMF, max(0, int(tentativas_msmf))),
        )
        erros = []
        for nome, backend, quantidade in estrategias:
            for tentativa in range(1, quantidade + 1):
                print(f'[CAMERA] Abrindo indice {self.camera_index}: '
                      f'{nome}, tentativa {tentativa}/{quantidade}...', flush=True)
                try:
                    self.cap = cv2.VideoCapture(self.camera_index, backend)
                    if self.cap is None or not self.cap.isOpened():
                        raise RuntimeError('Dispositivo nao abriu')
                    self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.largura)
                    self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.altura)
                    # Algumas cameras abrem sem conseguir entregar frames.
                    frame_valido = False
                    for _ in range(12):
                        ret, frame = self.cap.read()
                        if ret and frame is not None and getattr(frame, 'size', 0) > 0:
                            frame_valido = True
                            break
                        time.sleep(0.12)
                    if not frame_valido:
                        raise RuntimeError('Dispositivo abriu, mas nao forneceu frames')
                except Exception as exc:
                    erros.append(f'{nome} {tentativa}/{quantidade}: {exc}')
                    self.liberar(silencioso=True)
                    if pausa_entre_tentativas_s > 0:
                        time.sleep(pausa_entre_tentativas_s)
                    continue

                self.backend_ativo = nome
                resolucao_x = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                resolucao_y = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                print('\n===================================')
                print('Camera conectada com sucesso!')
                print(f'Indice da camera: {self.camera_index}')
                print(f'Backend: {nome}')
                print(f'Resolucao informada pelo driver: {resolucao_x} x {resolucao_y}')
                if (resolucao_x, resolucao_y) != (self.largura, self.altura):
                    print('[CAMERA][ATENCAO] Resolucao difere da solicitada; '
                          'verifique enquadramento e ROI na imagem gerada.')
                print('===================================\n')
                try:
                    self.estabilizar_camera()
                except Exception:
                    # Nao deixar a webcam reservada em caso de falha no warm-up.
                    self.liberar(silencioso=True)
                    raise
                return

        resumo = '; '.join(erros[-5:])
        raise RuntimeError(
            f'Nao foi possivel abrir a camera de indice {self.camera_index} '
            f'apos novas tentativas (DSHOW/MSMF). Detalhes: {resumo}. '
            'Verifique se outro aplicativo esta usando a webcam, cabo USB, '
            'permissoes do Windows ou indice do dispositivo.'
        )

    def estabilizar_camera(self):
        print('Estabilizando camera...')
        if self.cap is None or not self.cap.isOpened():
            raise RuntimeError('Camera nao aberta para estabilizacao')
        for _ in range(self.frames_estabilizacao):
            ok, frame = self.cap.read()
            if ok:
                self._publicar_preview(frame)
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            ret, frame = self.cap.read()
            if not ret or frame is None:
                time.sleep(0.1)
                continue
            self._publicar_preview(frame)
            if self.imagem_possui_luminosidade_adequada(frame):
                print('Camera estabilizada com sucesso!\n')
                return
            time.sleep(0.1)
        raise TimeoutError('Camera nao estabilizou a luminosidade em 30 segundos')

    def imagem_possui_luminosidade_adequada(self, frame):
        imagem_cinza = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        luminosidade_media = float(np.mean(imagem_cinza))
        print(f'Luminosidade atual: {luminosidade_media:.2f}')
        return luminosidade_media >= self.luminosidade_minima

    def aguardar_captura_automatica(self, tempo_espera=10.0):
        print('\n========== CAPTURA AUTOMATICA ==========')
        print('[CAMERA] Aguardando estabilizacao da amostra...')
        inicio = time.perf_counter()
        ultimo_segundo = -1
        while True:
            decorrido = time.perf_counter() - inicio
            segundo = int(decorrido)
            if segundo != ultimo_segundo:
                print(f'[DEBUG] Tempo decorrido: {decorrido:.1f} s', flush=True)
                ultimo_segundo = segundo
            if decorrido >= tempo_espera:
                break
            if self.on_frame is not None and self.cap is not None:
                ok, frame = self.cap.read()
                if ok:
                    self._publicar_preview(frame)
            time.sleep(0.05)
        print(f'[DEBUG] Espera automatica: {time.perf_counter() - inicio:.2f} s')
        print('[CAMERA] Atualizando frames apos espera automatica...')

    def capturar_imagens(self, quantidade=1, pasta_saida='imagens'):
        if self.cap is None or not self.cap.isOpened():
            raise RuntimeError('Camera indisponivel no momento da captura')
        os.makedirs(pasta_saida, exist_ok=True)
        self.aguardar_captura_automatica(tempo_espera=10.0)
        for _ in range(20):
            ret, frame = self.cap.read()
            if ret:
                self._publicar_preview(frame)
            if not ret:
                raise RuntimeError('Falha ao atualizar frames da camera')
        imagens_capturadas = []
        for indice in range(quantidade):
            ret, frame = self.cap.read()
            if not ret or frame is None:
                raise RuntimeError(f'Erro ao capturar imagem {indice+1}')
            caminho_imagem = os.path.join(pasta_saida, f'imagem_{indice + 1}.jpg')
            if not cv2.imwrite(caminho_imagem, frame):
                raise RuntimeError(f'Falha ao salvar imagem: {caminho_imagem}')
            imagens_capturadas.append(caminho_imagem)
            self._publicar_preview(frame)
            print(f'Imagem capturada: {caminho_imagem}')
            time.sleep(0.3)
        return imagens_capturadas

    def liberar(self, silencioso=False):
        cap = self.cap
        self.cap = None
        self.backend_ativo = None
        if cap is None:
            return
        try:
            cap.release()
            if not silencioso:
                print('Camera liberada com sucesso.')
        except Exception as exc:
            if not silencioso:
                print(f'[CAMERA] Problema ao liberar dispositivo: {exc}')
