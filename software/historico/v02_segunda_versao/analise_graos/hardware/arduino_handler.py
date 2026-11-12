import serial
import time

from models.sample import Sample


class ArduinoHandler:

    def __init__(
        self,
        port="COM4",
        baudrate=9600,
        timeout=1
    ):

        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout

        try:

            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout
            )

            # O Arduino normalmente reinicia
            # quando a porta serial é aberta.
            time.sleep(2)

            # Limpa mensagens antigas que possam
            # ter ficado no buffer.
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()

            print("\n===================================")
            print("Arduino conectado com sucesso!")
            print(f"Porta: {self.port}")
            print(f"Baudrate: {self.baudrate}")
            print("===================================\n")

        except serial.SerialException as erro:

            raise Exception(
                f"Erro ao conectar com o Arduino.\n"
                f"Porta: {self.port}\n"
                f"Detalhes: {erro}"
            )

    # =====================================================
    # INICIALIZAÇÃO
    # =====================================================

    def aguardar_arduino_pronto(self, timeout=15):

        print("Aguardando Arduino ficar pronto...")

        return self._aguardar_mensagem(
            "READY: Awaiting START command",
            timeout
        )

    def iniciar_processo(self):

        print("Iniciando processo de análise...")

        self._enviar_mensagem("START")

    # =====================================================
    # PROCESSO DA ANÁLISE
    # =====================================================

    def aguardar_comando_camera(self, timeout=60):

        print("Aguardando comando para captura da câmera...")

        return self._aguardar_mensagem(
            "CMD:ACTIVATE_CAMERA",
            timeout
        )

    def enviar_amostra_aprovada(self):

        print("Enviando resultado: AMOSTRA APROVADA")

        self._enviar_mensagem("CAM:SUCESSO")

    def enviar_amostra_reprovada(self):

        print("Enviando resultado: AMOSTRA REPROVADA")

        self._enviar_mensagem("CAM:RUIM")

    # =====================================================
    # DADOS FÍSICOS
    # =====================================================

    def obter_dados_fisicos_amostra(self, sample: Sample, timeout=120):
        """Lê peso inicial, temperatura, umidade e peso final.

        Aceita o protocolo novo do Arduino e o formato ambiental antigo.
        O timeout total contempla os 30 s de espera do firmware e movimentos.
        """
        inicio = time.monotonic()
        recebido = {"peso1": False, "temperatura": False,
                    "umidade": False, "peso2": False}

        while time.monotonic() - inicio < timeout:
            mensagem = self._ler_mensagem()
            if not mensagem:
                time.sleep(0.05)
                continue

            try:
                if mensagem.startswith("PESO:"):
                    sample.peso_inicial = float(mensagem.split(":", 1)[1])
                    recebido["peso1"] = True
                    print(f"[ARDUINO] Peso inicial: {sample.peso_inicial:.2f}")

                elif mensagem.startswith("PESO2:"):
                    sample.peso_final = float(mensagem.split(":", 1)[1])
                    recebido["peso2"] = True
                    print(f"[ARDUINO] Peso final: {sample.peso_final:.2f}")

                elif mensagem.startswith("TEMPERATURA:"):
                    sample.temperatura = float(mensagem.split(":", 1)[1])
                    recebido["temperatura"] = True
                    print(f"[ARDUINO] Temperatura: {sample.temperatura:.2f} °C")

                elif mensagem.startswith("UMIDADE:"):
                    sample.umidade = float(mensagem.split(":", 1)[1])
                    recebido["umidade"] = True
                    print(f"[ARDUINO] Umidade estimada: {sample.umidade:.2f} %")

                elif mensagem.startswith("Temperatura:") and "|" in mensagem:
                    partes = mensagem.split("|", 1)
                    sample.temperatura = float(partes[0].replace("Temperatura:", "").replace("°C", "").strip())
                    sample.umidade = float(partes[1].replace("Umidade Estimada:", "").replace("%", "").strip())
                    recebido["temperatura"] = recebido["umidade"] = True

                elif mensagem.startswith("ERRO:"):
                    raise RuntimeError(f"Arduino informou {mensagem}")

                elif mensagem == "FIM:1":
                    raise RuntimeError("Arduino terminou antes de todos os dados físicos serem recebidos")

            except ValueError:
                print(f"[ARDUINO] Valor inválido recebido: {mensagem}")

            if all(recebido.values()):
                print("\nTodos os dados físicos foram recebidos.")
                return sample

        faltantes = ", ".join(k for k, ok in recebido.items() if not ok)
        raise TimeoutError(
            f"Tempo limite excedido aguardando dados físicos: {faltantes}"
        )

    # =====================================================
    # FINALIZAÇÃO
    # =====================================================

    def aguardar_fim_processo(self, timeout=60):

        print(
            "Aguardando finalização do processo..."
        )

        return self._aguardar_mensagem(
            "FIM:1",
            timeout
        )

    # =====================================================
    # COMUNICAÇÃO SERIAL
    # =====================================================

    def _enviar_mensagem(self, mensagem):

        if not self.serial.is_open:

            raise ConnectionError(
                "A porta serial do Arduino "
                "está fechada."
            )

        mensagem_formatada = mensagem + "\n"

        self.serial.write(
            mensagem_formatada.encode("utf-8")
        )

        self.serial.flush()

        print(
            f"[PYTHON -> ARDUINO] "
            f"{mensagem}"
        )

    def _ler_mensagem(self):

        if not self.serial.is_open:

            raise ConnectionError(
                "A porta serial do Arduino "
                "está fechada."
            )

        if self.serial.in_waiting <= 0:

            return None

        try:

            mensagem = (
                self.serial.readline()
                .decode("utf-8", errors="ignore")
                .strip()
            )

            if mensagem:

                print(
                    f"[ARDUINO → PYTHON] "
                    f"{mensagem}"
                )

            return mensagem

        except Exception as erro:

            print(
                f"Erro ao ler mensagem serial: "
                f"{erro}"
            )

            return None

    def _aguardar_mensagem(
        self,
        mensagem_esperada,
        timeout
    ):

        inicio = time.time()

        while time.time() - inicio < timeout:

            mensagem = self._ler_mensagem()

            if mensagem is None:

                time.sleep(0.05)

                continue

            if mensagem == mensagem_esperada:

                return True

            # Mensagens inesperadas não encerram
            # automaticamente a espera.
            print(
                f"[SERIAL] Mensagem ignorada: "
                f"{mensagem}"
            )

        raise TimeoutError(
            f"Timeout aguardando: "
            f"{mensagem_esperada}"
        )

    # =====================================================
    # ENCERRAMENTO
    # =====================================================

    def fechar(self):

        if hasattr(self, "serial"):
            if self.serial.is_open:
                self.serial.close()
                print("Conexão com o Arduino encerrada.")