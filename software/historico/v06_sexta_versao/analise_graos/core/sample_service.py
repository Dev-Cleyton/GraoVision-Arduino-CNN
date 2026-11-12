from hardware.camera_handler import CameraHandler
from core.grain_classifier import GrainClassifier
from models.sample import Sample

import os
from dotenv import load_dotenv


# Carrega as variáveis do arquivo .env
load_dotenv()


class SampleService:

    def __init__(self):

        # Inicializa a câmera
        self.camera = CameraHandler()

        # Inicializa o classificador de grãos
        #self.classifier = GrainClassifier(
        #    api_key=os.getenv("ROBOFLOW_API_KEY"))
        api_key = os.getenv("ROBOFLOW_API_KEY")

        if api_key is None:

            raise Exception(
                "\nNão foi possível localizar a variável "
                "'ROBOFLOW_API_KEY' no arquivo .env.\n"
            )

        self.classifier = GrainClassifier(
            api_key=api_key
        )

    # =====================================================
    # MÉTODO PÚBLICO
    # =====================================================

    def analisar_amostra(self):

        """
        Executa todo o processo de análise visual
        da amostra.

        Retorna:
            Objeto Sample preenchido.
        """

        sample = self._criar_amostra()

        sample = self._capturar_imagens(sample)

        sample = self._classificar_graos(sample)

        sample = self._definir_resultado(sample)

        return sample

    # =====================================================
    # MÉTODOS PRIVADOS
    # =====================================================

    def _criar_amostra(self):

        """
        Cria um novo objeto Sample.
        """

        return Sample()

    def _capturar_imagens(self, sample):

        """
        Captura as imagens da amostra.
        """

        sample.imagens = self.camera.capturar_imagens()

        return sample

    def _classificar_graos(self, sample):

        """
        Executa a classificação dos grãos.
        """

        sample = self.classifier.classificar(sample)

        return sample

    def _definir_resultado(self, sample):

        """
        Define se a amostra foi aprovada ou não.
        """

        # Regra atual do projeto:
        # Mais de 25% de grãos ruins = REPROVADA.

        if sample.total_graos == 0 or sample.percentual_ruins > 25:

            sample.aprovado = False

        else:

            sample.aprovado = True

        return sample

    # =====================================================
    # FINALIZAÇÃO
    # =====================================================

    def liberar_recursos(self):

        """
        Libera todos os recursos utilizados
        pelo serviço.
        """

        self.camera.liberar()