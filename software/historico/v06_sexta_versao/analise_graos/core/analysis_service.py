from core.sample_service import SampleService
from hardware.arduino_handler import ArduinoHandler
from models.analysis_session import AnalysisSession
from models.analysis_result import AnalysisResult


class AnalysisService:

    def __init__(self):

        # =================================================
        # SERVIÇOS DO SISTEMA
        # =================================================

        self.sample_service = SampleService()
        self.arduino = ArduinoHandler()

    # =====================================================
    # MÉTODO PRINCIPAL
    # =====================================================

    def executar(self):

        """
        Executa um ciclo completo de análise.

        Fluxo:

            1. Aguarda Arduino
            2. Inicia processo
            3. Analisa amostra
            4. Envia resultado ao Arduino
            5. Se aprovada, coleta dados físicos
            6. Aguarda finalização
            7. Cria resultado
            8. Finaliza sessão
        """

        session = AnalysisSession()

        try:

            # ---------------------------------------------
            # 1. Inicialização
            # ---------------------------------------------

            self.iniciar_sistema()

            # ---------------------------------------------
            # 2. Análise visual
            # ---------------------------------------------

            sample = self.iniciar_analise()

            # Guarda a amostra na sessão imediatamente
            session.sample = sample

            # ---------------------------------------------
            # 3. Verifica resultado da IA
            # ---------------------------------------------

            if sample.aprovado:

                print("\nAmostra APROVADA pela IA.")

                # -----------------------------------------
                # 4. Coleta dados físicos
                # -----------------------------------------

                sample = self.obter_dados_fisicos(sample)

                session.sample = sample

                # -----------------------------------------
                # 5. Aguarda Arduino finalizar
                # -----------------------------------------

                self.finalizar_processo()

            else:

                print("\nAmostra REPROVADA pela IA.")

                # Quando a amostra é reprovada,
                # o Arduino retorna para IDLE.
                #
                # Portanto NÃO devemos tentar
                # coletar peso/temperatura/umidade.

            # ---------------------------------------------
            # 6. Cria resultado final
            # ---------------------------------------------

            result = self.criar_resultado(sample)

            session.analysis_result = result

            # ---------------------------------------------
            # 7. Finaliza sessão
            # ---------------------------------------------

            session.finalizar_sessao()

            return session

        except Exception as erro:

            print(
                f"\nErro durante a análise: {erro}"
            )

            session.registrar_erro(str(erro))

            return session

        finally:

            self.encerrar_sistema()

    # =====================================================
    # INICIALIZAÇÃO
    # =====================================================

    def iniciar_sistema(self):

        """
        Aguarda o Arduino informar que está pronto.
        """

        self.arduino.aguardar_arduino_pronto()

        print("\n" + "=" * 45)
        print("PREPARAÇÃO DA BALANÇA")
        print("=" * 45)
        print("Retire os grãos. Deixe apenas o recipiente, se utilizado.")
        input("Pressione ENTER para realizar a tara... ")
        self.arduino.realizar_tara()

        print("\nTara confirmada pelo Arduino.")
        print("Coloque a amostra de milho na posição de análise.")
        input("Pressione ENTER quando a amostra estiver pronta... ")

    # =====================================================
    # ANÁLISE DA AMOSTRA
    # =====================================================

    def iniciar_analise(self):

        """
        Inicia o processo no Arduino e executa
        a análise visual da amostra.
        """

        # ---------------------------------------------
        # Envia START
        # ---------------------------------------------

        self.arduino.iniciar_processo()

        # ---------------------------------------------
        # Aguarda posicionamento da amostra
        # ---------------------------------------------

        self.arduino.aguardar_comando_camera()

        # ---------------------------------------------
        # Executa análise visual
        # ---------------------------------------------

        sample = (
            self.sample_service
            .analisar_amostra()
        )

        # ---------------------------------------------
        # Envia resultado para Arduino
        # ---------------------------------------------
        if sample.aprovado:

            self.arduino.enviar_amostra_aprovada()

        else:

            self.arduino.enviar_amostra_reprovada()

        return sample

    # =====================================================
    # DADOS FÍSICOS
    # =====================================================

    def obter_dados_fisicos(self, sample):

        """
        Aguarda e coleta os dados físicos enviados
        pelo Arduino.

        Dados esperados:

            PESO
            Temperatura
            Umidade
            PESO2
        """

        print(
            "\nAguardando dados físicos "
            "da amostra..."
        )

        sample = (
            self.arduino
            .obter_dados_fisicos_amostra(
                sample
            )
        )

        return sample

    # =====================================================
    # FINALIZAÇÃO
    # =====================================================

    def finalizar_processo(self):

        """
        Aguarda o Arduino informar que terminou
        o processo físico.
        """

        self.arduino.aguardar_fim_processo()

    # =====================================================
    # RESULTADO
    # =====================================================

    def criar_resultado(self, sample):

        """
        Cria o objeto AnalysisResult
        com base nos dados da amostra.
        """

        resultado = AnalysisResult()

        resultado.aprovado = sample.aprovado

        resultado.percentual_graos_ruins = (
            sample.percentual_ruins
        )

        return resultado

    # =====================================================
    # ENCERRAMENTO
    # =====================================================

    def encerrar_sistema(self):

        """
        Libera os recursos utilizados
        durante a análise.
        """

        try:

            self.sample_service.liberar_recursos()

        except Exception as erro:

            print(
                f"Erro ao liberar recursos "
                f"do SampleService: {erro}"
            )

        try:

            self.arduino.fechar()

        except Exception as erro:

            print(
                f"Erro ao fechar Arduino: {erro}"
            )