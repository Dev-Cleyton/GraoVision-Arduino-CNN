from roboflow import Roboflow
class GrainClassifier:

    def __init__(
            self,
            api_key,
            project="kernel-corn",
            version=54
    ):

        if api_key is None:

            raise Exception(
                "\nA chave da API do Roboflow não foi encontrada."
                "\nVerifique o arquivo .env do projeto.\n"
            )

        rf = Roboflow(api_key=api_key)

        self.model = (
            rf.workspace()
            .project(project)
            .version(version)
            .model
        )

    def classificar(self, sample):

        total_graos = 0
        graos_ruins = 0

        for imagem in sample.imagens:

            resultado = (
                self.model
                .predict(
                    imagem,
                    confidence=28,
                    overlap=24
                )
                .json()
            )

            for predicao in resultado["predictions"]:

                total_graos += 1

                if predicao["class"] == "bad":

                    graos_ruins += 1

        if total_graos > 0:

            percentual_ruins = (
                    graos_ruins / total_graos
            ) * 100

        else:

            percentual_ruins = 0

        sample.total_graos = total_graos
        sample.graos_ruins = graos_ruins
        sample.percentual_ruins = percentual_ruins

        return sample