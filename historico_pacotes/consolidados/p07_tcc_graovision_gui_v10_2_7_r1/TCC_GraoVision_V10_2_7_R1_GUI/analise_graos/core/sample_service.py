"""Executa captura e classificação da V10, sem alterar a implementação da CNN."""
import os
from dotenv import load_dotenv
from hardware.camera_handler import CameraHandler
from core.grain_classifier import GrainClassifier

load_dotenv()


class SampleService:
    def __init__(self, camera=None, classifier=None, on_frame=None, on_image=None, camera_index=0):
        # Instanciar a camera somente quando o Arduino autorizar a captura.
        self.camera_index = camera_index
        self.on_frame = on_frame
        self.on_image = on_image
        self.camera = camera
        self.classifier = classifier

    def analisar_amostra(self, sample):
        if self.camera is None:
            kwargs = {'camera_index': self.camera_index}
            if self.on_frame is not None:
                kwargs['on_frame'] = self.on_frame
            self.camera = CameraHandler(**kwargs)
        if self.classifier is None:
            chave = os.getenv('ROBOFLOW_API_KEY')
            if not chave:
                raise RuntimeError('Defina ROBOFLOW_API_KEY no .env (nao distribuir esta chave)')
            self.classifier = GrainClassifier(api_key=chave)
        # Evita sobrescrever imagens entre ciclos.
        pasta = os.path.join('imagens', sample.id)
        sample.imagens = self.camera.capturar_imagens(quantidade=1, pasta_saida=pasta)
        self.classifier.classificar(sample)
        if self.on_image is not None and sample.roi_registros:
            registro = sample.roi_registros[-1]
            self.on_image(registro.get('imagem_anotada') or registro.get('imagem_roi'))
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
        camera = self.camera
        self.camera = None
        if camera is not None:
            camera.liberar()
