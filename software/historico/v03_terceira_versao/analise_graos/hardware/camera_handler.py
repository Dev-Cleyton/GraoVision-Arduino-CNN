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
        while True:

            ret, frame = self.cap.read()

            if not ret:
                continue

            if self.imagem_possui_luminosidade_adequada(frame):

                print("Câmera estabilizada com sucesso!\n")
                break

            time.sleep(0.1)

    # --------------------------------------------------
    # VERIFICAÇÃO DA IMAGEM
    # --------------------------------------------------

    def imagem_possui_luminosidade_adequada(self, frame):

        imagem_cinza = cv2.cvtColor(frame,cv2.COLOR_BGR2GRAY)

        luminosidade_media = np.mean(imagem_cinza)

        print( f"Luminosidade atual: " f"{luminosidade_media:.2f}")

        return (luminosidade_media >= self.luminosidade_minima)
    # --------------------------------------------------
    # CAPTURA DAS IMAGENS
    # --------------------------------------------------

    def capturar_imagens(self,quantidade=2,pasta_saida="imagens"):

        if not os.path.exists(pasta_saida):

            os.makedirs(pasta_saida)

        imagens_capturadas = []

        for indice in range(quantidade):

            ret, frame = self.cap.read()

            if not ret:

                print(
                    "Erro ao capturar a imagem."
                )

                continue

            caminho_imagem = os.path.join(pasta_saida,f"imagem_{indice + 1}.jpg")

            cv2.imwrite(caminho_imagem,frame)

            imagens_capturadas.append(caminho_imagem)

            print(f"Imagem capturada: "f"{caminho_imagem}")

            # Pequeno tempo entre as imagens.
            time.sleep(0.3)

        return imagens_capturadas

    # --------------------------------------------------
    # ENCERRAMENTO
    # --------------------------------------------------

    def liberar(self):

        if (self.cap is not None and self.cap.isOpened()):

            self.cap.release()

            print("Câmera liberada com sucesso.")