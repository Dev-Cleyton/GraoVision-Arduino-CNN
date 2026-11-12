"""Orquestracao fiel ao fluxo aprovado para V10.1 de validacao."""
import os
from pathlib import Path
from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parents[1] / ".env")

from core.sample_service import SampleService
from hardware.arduino_handler import ArduinoHandler, FalhaArduino
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
        camera_solicitada = False
        movimentos_reais = False
        armado = False
        processo_iniciado = False
        aquecimento_solicitado = os.getenv('AQUECIMENTO_REAL', '0').strip() == '1'
        try:
            movimentos_reais = os.getenv('MOVIMENTOS_REAIS', '0').strip() == '1'
            sample.aquecimento_solicitado = aquecimento_solicitado
            if aquecimento_solicitado and (not movimentos_reais or not interativo):
                raise RuntimeError('Aquecimento requer movimento real e autorizacao presencial')
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
                print('ATENCAO: ensaio mecanico; rele D7 bloqueado no firmware fornecido, '
                      'com parada de emergencia e percurso inspecionado.')
                confirmar = input('Digite ARMAR para autorizar movimentos fisicos deste ciclo: ').strip()
                if confirmar != 'ARMAR':
                    raise RuntimeError('Operador nao autorizou o movimento real')
                self.arduino.armar_movimentos_reais()
                armado = True
                if aquecimento_solicitado:
                    print('Antes de qualquer tentativa de armamento termico:')
                    print('conferir fusivel, fonte, lampada, circuito 12 V, termostato NF')
                    print('independente EM SERIE, corte DC dimensionado, materiais e E-stop.')
                    conf = input('Somente se inspecionado, digite INSPECAO-TERMICA: ').strip()
                    if conf != 'INSPECAO-TERMICA':
                        raise RuntimeError('Aquecimento cancelado pelo operador')
                    self.arduino.armar_aquecimento_apos_inspecao()
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
            processo_iniciado = True
            sample.peso_inicial = self.arduino.aguardar_p1_e_comando_camera()
            camera_solicitada = True
            # A camera so opera apos o comando do firmware, e P1 ja esta armazenado.
            # Erro de ROI, IA ou camera NAO significa 'graos ruins'.
            # O bloco externo de excecao envia STOP apenas uma vez.
            self.sample_service.analisar_amostra(sample)
            self.arduino.enviar_resultado_cnn(sample.status_cnn)
            camera_solicitada = False
            if sample.status_cnn == 'REPROVADA':
                self.arduino.aguardar_fim_reprovacao('REPROVADA')
                sample.status_final = 'REPROVADA_CNN'
                sample.motivo_final = sample.motivo_cnn
            elif sample.status_cnn == 'INCONCLUSIVA':
                self.arduino.aguardar_fim_reprovacao('INCONCLUSIVA')
                sample.status_final = 'INCONCLUSIVA_CNN'
                sample.motivo_final = sample.motivo_cnn
            elif sample.status_cnn == 'APROVADA':
                self.arduino.obter_serie_pesagens(sample, interativo=interativo)
                # Comando ON nao significa passagem de corrente nem valida umidade.
                sample.status_final = 'PENDENTE_VALIDACAO_UMIDADE'
                if sample.aquecimento_comandado:
                    sample.motivo_final = (
                        'CNN aprovada; rele D7 comandado durante ensaio e tempo registrado. '
                        'Corrente e temperatura real da amostra nao comprovadas; '
                        'umidade dos graos nao determinada por metodo de referencia.'
                    )
                else:
                    sample.motivo_final = (
                        'CNN aprovada; periodo de monitoramento sem aquecimento comandado. '
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
            # Causa original sempre preservada, inclusive se retorno falhar.
            session.registrar_erro(str(exc))
            sample.status_final = 'ERRO_PROCESSO'
            sample.motivo_final = str(exc)
            falha_confirmada = isinstance(exc, FalhaArduino)
            if falha_confirmada:
                sample.codigo_erro_arduino = exc.codigo
            elif camera_solicitada:
                # Erro do Python/camera: informar ao UNO, que ainda espera CAM.
                try:
                    confirmado = self.arduino.notificar_falha_camera()
                    sample.codigo_erro_arduino = confirmado.codigo
                    falha_confirmada = True
                except Exception as aviso:
                    sample.eventos_recuperacao.append(
                        'AVISO:CAM_ERRO_NAO_CONFIRMADO:' + str(aviso))
            if falha_confirmada:
                # O Arduino ja parou e persistiu causa; NAO enviar STOP,
                # pois sobrescreveria a causa no journal e pode cortar D6.
                try:
                    self.arduino.retorno_apos_erro(
                        sample, presencial=(interativo and movimentos_reais))
                except Exception as retorno_exc:
                    sample.recuperacao_status = 'FALHA_COMUNICACAO_RETORNO'
                    sample.eventos_recuperacao.append(str(retorno_exc))
            else:
                # Ex.: cancelamento no armamento/recuperacao, porta fechada.
                # Nao enviar STOP quando nunca houve armamento/movimento:
                # o Arduino pode estar apenas solicitando referencia EEPROM.
                if 'EEPROM requer verificacao' in str(exc) or \
                   'Recuperacao de posicao nao autorizada' in str(exc):
                    sample.recuperacao_status = 'REFERENCIA_PENDENTE_MANUAL'
                elif armado or processo_iniciado:
                    try:
                        self.arduino.parar()
                    except Exception:
                        pass
        finally:
            # Mesmo sem PF, P1 e P2 validos permitem auditar o offset entre
            # posicoes. Nunca inventar PF ou perda de massa apos erro.
            if sample.peso_inicial is not None and sample.peso_referencia is not None:
                sample.calcular_variacao_massa()
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
