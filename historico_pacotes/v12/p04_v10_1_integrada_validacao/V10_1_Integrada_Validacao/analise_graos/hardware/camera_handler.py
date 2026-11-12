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
            luminosidade_minima=65.94
    ):

        self.camera_index = camera_index
        self.largura = largura
        self.altura = altura
        self.frames_estabilizacao = frames_estabilizacao
        self.luminosidade_minima = luminosidade_minima

        self.cap = cv2.VideoCapture(self.camera_index,cv2.CAP_DSHOW)

        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH,self.largura)

        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT,self.altura)

        if not self.cap.isOpened():

            raise Exception(
                f"Não foi possível abrir a câmera "
                f"de índice {self.camera_index}."
            )

        print("\n===================================")
        print("Câmera conectada com sucesso!")
        print(f"Índice da câmera: {self.camera_index}")
        print(f"Resolução: {self.largura} x {self.altura}")
        print("===================================\n")

        # Aguarda a estabilização da câmera.
        self.estabilizar_camera()

    # --------------------------------------------------
    # ESTABILIZAÇÃO DA CÂMERA
    # --------------------------------------------------

    def estabilizar_camera(self):

        print("Estabilizando câmera...")

        # Descarta os primeiros frames.
        for _ in range(self.frames_estabilizacao):

            self.cap.read()

        # Aguarda até que a luminosidade fique adequada.
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:

            ret, frame = self.cap.read()

            if not ret:
                time.sleep(0.1)
                continue

            if self.imagem_possui_luminosidade_adequada(frame):

                print("Câmera estabilizada com sucesso!\n")
                return

            time.sleep(0.1)

        raise TimeoutError("Camera nao estabilizou a luminosidade em 30 segundos")

    # --------------------------------------------------
    # VERIFICAÇÃO DA IMAGEM
    # --------------------------------------------------

    def imagem_possui_luminosidade_adequada(self, frame):

        imagem_cinza = cv2.cvtColor(frame,cv2.COLOR_BGR2GRAY)

        luminosidade_media = np.mean(imagem_cinza)

        print( f"Luminosidade atual: " f"{luminosidade_media:.2f}")

        return (luminosidade_media >= self.luminosidade_minima)
    # --------------------------------------------------
    # CAPTURA AUTOMATICA: ESPERA COM CRONOMETRO
    # --------------------------------------------------

    def aguardar_captura_automatica(self, tempo_espera=10.0):
        """Espera apos CMD:ACTIVATE_CAMERA e exibe o cronometro no console."""
        print("\n========== CAPTURA AUTOMATICA ==========")
        print("[CAMERA] Aguardando estabilizacao da amostra...")
        inicio = time.perf_counter()
        ultimo_segundo = -1
        while True:
            decorrido = time.perf_counter() - inicio
            segundo = int(decorrido)
            if segundo != ultimo_segundo:
                print(f"[DEBUG] Tempo decorrido: {decorrido:.1f} s", flush=True)
                ultimo_segundo = segundo
            if decorrido >= tempo_espera:
                break
            time.sleep(0.05)
        print(f"[DEBUG] Espera automatica: {time.perf_counter() - inicio:.2f} s")
        print("[CAMERA] Atualizando frames apos espera automatica...")

    # --------------------------------------------------
    # --------------------------------------------------
    # CAPTURA DAS IMAGENS
    # --------------------------------------------------

    def capturar_imagens(self,quantidade=1,pasta_saida="imagens"):

        if not os.path.exists(pasta_saida):

            os.makedirs(pasta_saida)

        # Inicia a espera apos a autorizacao de captura do Arduino.
        self.aguardar_captura_automatica(tempo_espera=10.0)

        # Descartar frames pendentes apos a espera automatica.
        # CAP_DSHOW nao garante que todo buffer antigo seja eliminado.
        for _ in range(20):
            ret, _ = self.cap.read()
            if not ret:
                raise RuntimeError("Falha ao atualizar frames da camera.")

        imagens_capturadas = []

        for indice in range(quantidade):

            ret, frame = self.cap.read()

            if not ret:

                print(
                    "Erro ao capturar a imagem."
                )

                continue

            caminho_imagem = os.path.join(pasta_saida,f"imagem_{indice + 1}.jpg")

            if not cv2.imwrite(caminho_imagem,frame):
                raise RuntimeError(f"Falha ao salvar imagem: {caminho_imagem}")

            imagens_capturadas.append(caminho_imagem)

            print(f"Imagem capturada: "f"{caminho_imagem}")

            # Pequeno tempo entre as imagens.
            time.sleep(0.3)

        if len(imagens_capturadas) != quantidade:
            raise RuntimeError("Captura incompleta: nao foi possivel obter todas as imagens.")

        return imagens_capturadas

    # --------------------------------------------------
    # ENCERRAMENTO
    # --------------------------------------------------

    def liberar(self):

        if (self.cap is not None and self.cap.isOpened()):

            self.cap.release()

            print("Câmera liberada com sucesso.")