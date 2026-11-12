"""Dados da amostra. Umidade de graos so quando medida por metodo validado."""
from datetime import datetime
from uuid import uuid4


class Sample:
    def __init__(self):
        self.id = datetime.now().strftime('%Y%m%d_%H%M%S_') + uuid4().hex[:6]
        self.imagens = []
        self.classes_detectadas = {}
        self.roi_modo = 'AUTOMATICA_BANDEJA_V2'
        self.roi_status = 'NAO_EXECUTADA'
        self.roi_registros = []  # centro, raio, cobertura, suporte e comparacao ROI fixa
        self.total_graos = 0
        self.graos_ruins = 0
        self.percentual_ruins = 0.0
        self.cnn_aprovada = False
        self.status_cnn = 'NAO_EXECUTADA'
        self.motivo_cnn = ''
        self.peso_inicial = None  # P1, antes de movimentar
        self.peso_referencia = None  # P2, ja na posicao de secagem
        self.peso_final = None  # PF, ultimo peso na mesma posicao de P2
        self.delta_posicionamento_g = None  # P2-P1: diagnostico, nao perda de massa
        self.temperatura_local = None  # NTC proximo da celula, nao temperatura da camara
        self.medicoes = []  # dict tempo_s,peso_g,temp_ntc_c
        self.movimentos = []  # telemetria de pulsos realmente comandados pelo Arduino
        self.perda_massa_g = None
        self.perda_massa_percentual = None
        self.umidade_final = None  # NUNCA preencher com temperatura/NTC/perda de massa
        self.limite_umidade_empresa = None
        self.metodo_umidade = None
        self.status_final = 'EM_ANDAMENTO'
        self.motivo_final = ''
        self.aprovado = False
        self.aquecimento_realizado = False
        self.movimento_simulado = True

    def calcular_variacao_massa(self):
        if self.peso_referencia is not None and self.peso_inicial is not None:
            self.delta_posicionamento_g = self.peso_referencia - self.peso_inicial
        if self.peso_referencia is not None and self.peso_final is not None:
            if self.peso_referencia <= 0:
                raise ValueError('P2 deve ser maior que zero')
            self.perda_massa_g = self.peso_referencia - self.peso_final
            self.perda_massa_percentual = 100 * self.perda_massa_g / self.peso_referencia

    def avaliar_umidade_validada(self, umidade, limite, metodo):
        """Permite adicionar posteriormente resultado de um metodo metrologicamente validado.
        Sem dados de referencia, a aprovacao final permanece pendente.
        """
        if not self.cnn_aprovada:
            raise ValueError('Nao avaliar umidade em amostra reprovada ou inconclusiva na CNN')
        if not metodo or not metodo.strip():
            raise ValueError('Informe o metodo validado de determinacao de umidade')
        u, l = float(umidade), float(limite)
        if not (0 <= u <= 100 and 0 <= l <= 100):
            raise ValueError('Umidade e limite devem estar entre 0 e 100%')
        self.umidade_final = u
        self.limite_umidade_empresa = l
        self.metodo_umidade = metodo.strip()
        if u > l:
            self.status_final = 'REPROVADA_UMIDADE'
            self.motivo_final = f'Umidade validada de {u:.2f}% acima do limite de {l:.2f}%'
            self.aprovado = False
        else:
            self.status_final = 'APROVADA'
            self.motivo_final = f'CNN aprovada e umidade validada de {u:.2f}% dentro do limite de {l:.2f}%'
            self.aprovado = True
