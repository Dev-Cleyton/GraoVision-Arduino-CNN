"""Orquestracao fiel ao fluxo aprovado para V10.1 de validacao."""
import os
from pathlib import Path
from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parents[1] / ".env")

from core.sample_service import SampleService
from hardware.arduino_handler import ArduinoHandler
from models.analysis_session import AnalysisSession
from models.analysis_result import AnalysisResult
from infrastructure.csv_service import salvar_relatorio


class AnalysisService:
    def __init__(self, arduino=None, sample_service=None):
        self.arduino = arduino if arduino is not None else ArduinoHandler()
        self.sample_service = sample_service if sample_service is not None else SampleService()

    def executar(self, interativo=True, salvar=True):
        session = AnalysisSession()
        sample = session.sample
        session.sample = sample
        try:
            movimentos_reais = os.getenv('MOVIMENTOS_REAIS', '0').strip() == '1'
            # Nunca recuperar automaticamente; confirma a origem mecanica.
            if movimentos_reais and not interativo:
                raise RuntimeError('Movimento real exige operador presencial')
            self.arduino.aguardar_pronto(recuperar_interativamente=movimentos_reais)
            if interativo:
                print('\nAntes da tara: base em X inferior, percurso livre, prato vazio,')
                print('servo com acesso livre ao centro (90 graus).')
                input('Confirme as condicoes e pressione ENTER... ')
            if movimentos_reais:
                if not interativo:
                    raise RuntimeError('Movimento real exige supervisao presencial')
                print('ATENCAO: somente ensaio mecanico SEM aquecedor, '
                      'com parada de emergencia e percurso inspecionado.')
                confirmar = input('Digite ARMAR para autorizar movimentos fisicos deste ciclo: ').strip()
                if confirmar != 'ARMAR':
                    raise RuntimeError('Operador nao autorizou o movimento real')
                self.arduino.armar_movimentos_reais()
            # Obrigatorio em modo real e simulado: nao tarar sem centralizar.
            self.arduino.centralizar_antes_tara()
            # O Arduino R3 so envia SERVO:CENTRO:OK apos acomodacao.
            # Confirmacao extra impede tara enquanto operador ainda ve vibracao.
            if movimentos_reais and interativo:
                print('\n[SERVO] Centro concluido. Aguarde a bandeja parar completamente.')
                input('Prato VAZIO e estabilizado? Pressione ENTER para autorizar a TARA... ')
            self.arduino.realizar_tara()
            if interativo:
                input('Coloque os graos no prato. Pressione ENTER para iniciar... ')
            self.arduino.iniciar_processo()
            sample.peso_inicial = self.arduino.aguardar_p1_e_comando_camera()
            # A camera so opera apos o comando do firmware, e P1 ja esta armazenado.
            # Erro de ROI, IA ou camera NAO significa 'graos ruins'.
            # O bloco externo de excecao envia STOP apenas uma vez.
            self.sample_service.analisar_amostra(sample)
            self.arduino.enviar_resultado_cnn(sample.status_cnn)
            if sample.status_cnn == 'REPROVADA':
                self.arduino.aguardar_fim_reprovacao('REPROVADA')
                sample.status_final = 'REPROVADA_CNN'
                sample.motivo_final = sample.motivo_cnn
            elif sample.status_cnn == 'INCONCLUSIVA':
                self.arduino.aguardar_fim_reprovacao('INCONCLUSIVA')
                sample.status_final = 'INCONCLUSIVA_CNN'
                sample.motivo_final = sample.motivo_cnn
            elif sample.status_cnn == 'APROVADA':
                self.arduino.obter_serie_pesagens(sample)
                # Simulacao de monitoramento NUNCA comprova secagem nem umidade.
                sample.status_final = 'PENDENTE_VALIDACAO_UMIDADE'
                sample.motivo_final = (
                    'CNN aprovada; periodo de monitoramento sem aquecimento real. '
                    'Umidade dos graos nao determinada por metodo validado.'
                )
            else:
                raise RuntimeError(f'Resultado CNN inconsistente: {sample.status_cnn}')
            result = AnalysisResult()
            result.aprovado = sample.aprovado
            result.mensagem = sample.motivo_final
            result.status_final = sample.status_final
            result.percentual_graos_ruins = sample.percentual_ruins
            session.analysis_result = result
        except Exception as exc:
            session.registrar_erro(str(exc))
            sample.status_final = 'ERRO_PROCESSO'
            sample.motivo_final = str(exc)
            try:
                self.arduino.parar()
            except Exception:
                pass
        finally:
            session.finalizar_sessao()
            if salvar:
                try:
                    salvar_relatorio(session)
                except Exception as exc:
                    print(f'[RELATORIO] Falha ao gravar arquivos: {exc}')
                    if not session.possui_erro:
                        session.registrar_erro(f'Nao foi possivel salvar relatorio: {exc}')
            try:
                self.sample_service.liberar_recursos()
            except Exception as exc:
                print(f'Falha ao liberar camera: {exc}')
            self.arduino.fechar()
        return session
