class Sample:

    def __init__(self):

        # Identificação da análise
        self.id = None

        # Imagens
        self.imagens = []

        # Dados da IA
        self.total_graos = 0
        self.graos_ruins = 0
        self.percentual_ruins = 0

        # Dados físicos
        self.peso_inicial = 0
        self.peso_final = 0

        self.temperatura = 0
        self.umidade = 0

        # Resultado final
        self.aprovado = False