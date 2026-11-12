from core.sample_service import SampleService

#from analise_graos.hardware.arduino_handler import ArduinoHandler
from hardware.arduino_handler import ArduinoHandler
from models.analysis_session import AnalysisSession
from models.analysis_result import AnalysisResult


class AnalysisService:

    def __init__(self):

        # Serviços do sistema
        self.sample_service = SampleService()
        self.arduino = ArduinoHandler()

    # =====================================================
    # MÉTODO PÚBLICO
    # =====================================================

    def executar(self):

        """
        Executa um ciclo completo de análise.

        Retorna:
            AnalysisSession
        """

        session = AnalysisSession()

        try:

            self.iniciar_sistema()

            sample = self.iniciar_analise()

            sample = self.obter_dados_fisicos(sample)

            self.finalizar_processo()

            result = self.criar_resultado(sample)

            session.sample = sample
            session.analysis_result = result

            session.finalizar_sessao()

            return session

        except Exception as erro:

            session.registrar_erro(str(erro))

            return session

        finally:

            self.encerrar_sistema()

    # =====================================================
    # ETAPAS DO PROCESSO
    # =====================================================

    def iniciar_sistema(self):

        """
        Aguarda o Arduino ficar pronto.
        """

        self.arduino.aguardar_arduino_pronto()

    def iniciar_analise(self):

        """
        Inicia o processo no Arduino e realiza
        a análise visual da amostra.
        """

        # Envia START para o Arduino
        self.arduino.iniciar_processo()

        # Aguarda o comando da câmera
        self.arduino.aguardar_comando_camera()

        # Executa toda a análise visual
        sample = self.sample_service.analisar_amostra()

        # Informa ao Arduino o resultado
        if sample.aprovada:

            self.arduino.enviar_amostra_aprovada()

        else:

            self.arduino.enviar_amostra_reprovada()

        return sample

    def obter_dados_fisicos(self, sample):

        """
        Obtém todos os dados físicos do processo.
        """

        sample.peso_inicial = (
            self.arduino.ler_peso_inicial()
        )

        sample.peso_final = (
            self.arduino.ler_peso_final()
        )

        sample.temperatura = (
            self.arduino.ler_temperatura()
        )

        sample.umidade = (
            self.arduino.ler_umidade()
        )

        return sample

    def finalizar_processo(self):

        """
        Aguarda a finalização do processo.
        """

        self.arduino.aguardar_fim_processo()

    def criar_resultado(self, sample):

        """
        Cria o resultado final da análise.
        """

        resultado = AnalysisResult()

        resultado.aprovado = sample.aprovada

        resultado.percentual_graos_ruins = (
            sample.percentual_ruins
        )

        return resultado

    def encerrar_sistema(self):

        """
        Libera todos os recursos do sistema.
        """

        self.sample_service.liberar_recursos()

        self.arduino.fechar()