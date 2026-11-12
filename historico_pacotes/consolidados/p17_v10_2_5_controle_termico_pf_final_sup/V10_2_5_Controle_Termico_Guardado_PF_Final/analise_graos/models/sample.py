"""Dados da amostra. Umidade de graos so quando medida por metodo validado."""
from datetime import datetime
from uuid import uuid4
from core.balanco_massa import calcular_balanco


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
        self.temperatura_local = None  # alias legado; NTC A2 agora esta na camara
        self.temperatura_camara = None  # NTC A2 realocado, sem validacao de referencia
        self.estado_protecao_termica = "NAO_CONFIRMADA"
        self.medicoes = []  # dict tempo_s,peso_g,temp_ntc_c
        self.eventos_servo_pwm = []  # suspendido antes de P2; retomado depois de PF
        self.movimentos = []  # telemetria de pulsos realmente comandados pelo Arduino
        self.codigo_erro_arduino = None
        self.recuperacao_status = 'NAO_APLICAVEL'
        self.eventos_recuperacao = []
        self.perda_massa_g = None  # delta aparente P2-PF (assinatura preservada)
        self.perda_massa_percentual = None  # base P2; compatibilidade com R3
        self.diferenca_inicial_p1_menos_p2_g = None
        self.peso_final_corrigido_g = None  # mesma referencia de P1
        self.perda_massa_percentual_ref_p1 = None
        self.tendencia_leitura_massa = None
        self.balanco_massa_detalhado = None
        self.umidade_final = None  # NUNCA preencher com temperatura/NTC/perda de massa
        self.limite_umidade_empresa = None
        self.metodo_umidade = None
        self.status_final = 'EM_ANDAMENTO'
        self.motivo_final = ''
        self.aprovado = False
        self.aquecimento_realizado = False  # legado: ON COMANDADO, nao corrente real
        self.aquecimento_solicitado = False
        self.aquecimento_comandado = False
        self.eventos_aquecedor = []
        self.tempo_comando_on_ms = None
        self.motivo_desligamento_aquecedor = None
        self.medicoes_resfriamento = []
        self.movimento_simulado = True

    def calcular_variacao_massa(self):
        # P1 e P2 sempre preservados como leituras brutas; nao re-tarar ou
        # alterar peso anterior para "forcar" perda positiva.
        if self.peso_referencia is not None and self.peso_inicial is not None:
            self.delta_posicionamento_g = float(self.peso_referencia)-float(self.peso_inicial)
            self.diferenca_inicial_p1_menos_p2_g = -self.delta_posicionamento_g
        if all(v is not None for v in (self.peso_inicial, self.peso_referencia, self.peso_final)):
            resultado = calcular_balanco(self.peso_inicial, self.peso_referencia, self.peso_final)
            self.balanco_massa_detalhado = resultado
            self.delta_posicionamento_g = resultado['offset_posicao_P2_menos_P1_g']
            self.diferenca_inicial_p1_menos_p2_g = resultado['diferenca_inicial_P1_menos_P2_g']
            self.peso_final_corrigido_g = resultado['PF_corrigido_referencia_P1_g']
            self.perda_massa_g = resultado['perda_massa_aparente_g']
            self.perda_massa_percentual = resultado['perda_massa_aparente_percentual_ref_P2']
            self.perda_massa_percentual_ref_p1 = resultado['perda_massa_aparente_percentual_ref_P1']
            self.tendencia_leitura_massa = resultado['tendencia_leitura']

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
