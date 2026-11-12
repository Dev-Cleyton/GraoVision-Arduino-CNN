"""Executa captura e classificação da V10, sem alterar a implementação da CNN."""
import os
from dotenv import load_dotenv
from hardware.camera_handler import CameraHandler
from core.grain_classifier import GrainClassifier

load_dotenv()


class SampleService:
    def __init__(self, camera=None, classifier=None):
        # Instanciar a camera somente quando o Arduino autorizar a captura.
        self.camera = camera
        self.classifier = classifier

    def analisar_amostra(self, sample):
        if self.camera is None:
            self.camera = CameraHandler()
        if self.classifier is None:
            chave = os.getenv("clsROBOFLOW_API_KEY")
            if not chave:
                raise RuntimeError('Defina ROBOFLOW_API_KEY no .env (nao distribuir esta chave)')
            self.classifier = GrainClassifier(api_key=chave)
        # Evita sobrescrever imagens entre ciclos.
        pasta = os.path.join('imagens', sample.id)
        sample.imagens = self.camera.capturar_imagens(quantidade=1, pasta_saida=pasta)
        self.classifier.classificar(sample)
        if sample.total_graos == 0:
            sample.status_cnn = 'INCONCLUSIVA'
            sample.motivo_cnn = 'Nenhuma deteccao valida dentro da regiao analisada'
            sample.cnn_aprovada = False
        elif sample.percentual_ruins > 25.0:
            sample.status_cnn = 'REPROVADA'
            sample.motivo_cnn = (
                f'{sample.graos_ruins} de {sample.total_graos} deteccoes '
                f'({sample.percentual_ruins:.2f}%) classificadas como bad; limite 25%'
            )
            sample.cnn_aprovada = False
        else:
            sample.status_cnn = 'APROVADA'
            sample.motivo_cnn = (
                f'{sample.graos_ruins} de {sample.total_graos} deteccoes bad '
                f'({sample.percentual_ruins:.2f}%), dentro do limite visual de 25%'
            )
            sample.cnn_aprovada = True
        return sample

    def liberar_recursos(self):
        if self.camera is not None:
            self.camera.liberar()
