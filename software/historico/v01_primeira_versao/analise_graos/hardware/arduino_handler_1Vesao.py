import serial
import time

from models.sample import Sample

class ArduinoHandler:

    def __init__(self, port="COM4", baudrate=9600, timeout=1):

        try:
            self.serial = serial.Serial(
                port=port,
                baudrate=baudrate,
                timeout=timeout
            )

            # Tempo para o Arduino reiniciar após abrir a serial
            time.sleep(2)

            print("\n===================================")
            print("Arduino conectado com sucesso!")
            print(f"Porta: {port}")
            print(f"Baudrate: {baudrate}")
            print("===================================\n")

        except Exception as erro:

            raise Exception(
                f"Erro ao conectar com o Arduino.\n"
                f"Detalhes: {erro}"
            )

    # =====================================================
    # INICIALIZAÇÃO DO SISTEMA
    # =====================================================

    def aguardar_arduino_pronto(self):

        print("Aguardando Arduino ficar pronto...")

        return self._aguardar_mensagem("READY: Awaiting START command")

    def iniciar_processo(self):

        print("Iniciando processo de análise...")

        self._enviar_mensagem("START")

    # =====================================================
    # PROCESSO DA ANÁLISE
    # =====================================================

    #def aguardar_amostra_pronta(self):
    def aguardar_comando_camera(self):

        print("Aguardando amostra ser posicionada...")

        return self._aguardar_mensagem("CMD:ACTIVATE_CAMERA")

    def enviar_amostra_aprovada(self):

        print("Enviando resultado: AMOSTRA APROVADA")

        self._enviar_mensagem("CAM:SUCESSO")

    def enviar_amostra_reprovada(self):

        print("Enviando resultado: AMOSTRA REPROVADA")

        self._enviar_mensagem("CAM:RUIM")

    # =====================================================
    # DADOS FÍSICOS DA AMOSTRA
    # =====================================================

    def obter_dados_fisicos_amostra(self, sample: Sample):

        """
        Futuramente este método será responsável
        por preencher todos os dados físicos da amostra.
        """

        # IMPLEMENTAR NA FASE 2

        return sample

    # =====================================================
    # FINALIZAÇÃO DO PROCESSO
    # =====================================================

    def aguardar_fim_processo(self):

        print("Aguardando finalização do processo...")

        return self._aguardar_mensagem("FIM:1")

    # =====================================================
    # MÉTODOS INTERNOS
    # =====================================================

    def _enviar_mensagem(self, mensagem):

        mensagem += "\n"

        self.serial.write(mensagem.encode())

    def _aguardar_mensagem(self, mensagem_esperada):

        while True:

            if self.serial.in_waiting > 0:

                mensagem_recebida = (
                    self.serial.readline()
                    .decode()
                    .strip()
                )

                print(f"[ARDUINO] {mensagem_recebida}")

                if mensagem_recebida == mensagem_esperada:
                    return True

    # =====================================================
    # ENCERRAMENTO
    # =====================================================

    def fechar(self):

        if self.serial.is_open:

            self.serial.close()

            print("Conexão com o Arduino encerrada.")