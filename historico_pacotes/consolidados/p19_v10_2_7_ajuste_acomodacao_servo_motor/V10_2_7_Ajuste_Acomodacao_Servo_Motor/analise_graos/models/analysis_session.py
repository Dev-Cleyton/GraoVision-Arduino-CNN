from datetime import datetime

from models.sample import Sample
from models.analysis_result import AnalysisResult


class AnalysisSession:

    def __init__(self):

        # Identificação da sessão
        self.session_id = None

        # Horários
        self.data_hora_inicio = datetime.now()
        self.data_hora_fim = None
        self.tempo_total_segundos = 0

        # Objetos do sistema
        self.sample = Sample()
        self.analysis_result = AnalysisResult()

        # Status da sessão
        self.status = "EM_ANDAMENTO"

        # Controle de erros
        self.possui_erro = False
        self.mensagem_erro = None

        # Observações adicionais
        self.observacoes = None

    # -------------------------------------------------
    # Controle da Sessão
    # -------------------------------------------------

    def registrar_erro(self, mensagem):
        """
        Registra um erro ocorrido durante a análise.
        """
        self.possui_erro = True
        self.mensagem_erro = mensagem
        self.status = "ERRO"

    def finalizar_sessao(self):
        """
        Finaliza a sessão de análise e calcula o tempo total.
        """
        self.data_hora_fim = datetime.now()

        self.tempo_total_segundos = (
            self.data_hora_fim - self.data_hora_inicio
        ).total_seconds()

        # Atualiza o status somente se não houver erro.
        if not self.possui_erro:
            self.status = "FINALIZADA"