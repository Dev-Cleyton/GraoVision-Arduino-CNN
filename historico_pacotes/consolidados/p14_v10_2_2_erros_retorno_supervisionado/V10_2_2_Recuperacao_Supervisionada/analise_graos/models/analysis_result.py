from datetime import datetime


class AnalysisResult:
    def __init__(self):
        self.aprovado = False
        self.mensagem = ''
        self.status_final = 'EM_ANDAMENTO'
        self.percentual_graos_ruins = 0.0
        self.data_hora_analise = datetime.now().isoformat(timespec='seconds')
        self.tempo_total_processamento = 0
