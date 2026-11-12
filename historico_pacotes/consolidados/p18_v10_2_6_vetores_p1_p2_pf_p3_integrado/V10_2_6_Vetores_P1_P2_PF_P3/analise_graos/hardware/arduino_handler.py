"""Cliente serial V10.1. Nao confundir MEDICAO com umidade de graos."""
import math
import time
import serial


class FalhaArduino(RuntimeError):
    """Falha que o Arduino confirmou: nunca sobrescrever sua causa com STOP."""
    def __init__(self, linha):
        self.linha = linha
        self.codigo = linha.split(':', 1)[1] if ':' in linha else linha
        super().__init__(f'Falha do Arduino: {linha}')


class ArduinoHandler:
    def __init__(self, port='COM4', baudrate=9600, timeout=0.5, serial_obj=None):
        self.port = port
        self.baudrate = baudrate
        self.serial = serial_obj if serial_obj is not None else serial.Serial(
            port=port, baudrate=baudrate, timeout=timeout
        )
        if serial_obj is None:
            time.sleep(2)  # abertura da COM reinicia normalmente o Arduino Uno

    def _escrever(self, comando):
        if not self.serial.is_open:
            raise ConnectionError('Porta serial fechada')
        self.serial.write((comando + '\n').encode('ascii'))
        self.serial.flush()
        print(f'[PC -> UNO] {comando}')

    def _ler(self):
        if not self.serial.is_open:
            raise ConnectionError('Porta serial fechada')
        linha = self.serial.readline().decode('utf-8', errors='replace').strip()
        if linha:
            print(f'[UNO -> PC] {linha}')
            self._validar_erro(linha)
        return linha

    @staticmethod
    def _validar_erro(linha):
        if linha.startswith('ERRO:') or linha == 'TARA:ERRO':
            raise FalhaArduino(linha)

    @staticmethod
    def _numero(texto, nome):
        numero = float(texto)
        if not math.isfinite(numero):
            raise ValueError(f'{nome} nao finito')
        return numero

    def _esperar(self, esperado, timeout=30):
        fim = time.monotonic() + timeout
        while time.monotonic() < fim:
            linha = self._ler()
            if linha == esperado:
                return linha
            if linha == 'FIM:1' or linha.startswith('FIM:'):
                raise RuntimeError(f'Fim inesperado ao aguardar {esperado}: {linha}')
        raise TimeoutError(f'Timeout aguardando {esperado}')

    def aguardar_pronto(self, timeout=20, recuperar_interativamente=False):
        """Se EEPROM requer referencia, somente operador presente pode confirmar X.

        RECUPERAR:X nao movimenta o hardware; grava a origem logica apos
        verificacao fisica obrigatoria do elevador inferior e servo centro.
        """
        fim = time.monotonic() + timeout
        while time.monotonic() < fim:
            linha = self._ler()
            if linha == 'READY:V10.1':
                return
            if linha.startswith('RECUPERACAO:PENDENTE:'):
                if not recuperar_interativamente:
                    raise RuntimeError('EEPROM requer verificacao fisica em X; inicio bloqueado')
                print('\n[EEPROM] POSICAO DESCONHECIDA: nao ha homing automatico.')
                print('Nao acione RECUPERAR:X se o prato NAO estiver realmente em X inferior,')
                print('com o servo fisicamente no centro, sem graos e com percurso livre.')
                confirmacao = input('Apos CONFERIR FISICAMENTE, digite CONFIRMO-X: ').strip()
                if confirmacao != 'CONFIRMO-X':
                    raise RuntimeError('Recuperacao de posicao nao autorizada pelo operador')
                self._escrever('RECUPERAR:X')
                # O Arduino responde com ACK e depois READY:V10.1.
                fim = time.monotonic() + 20
        raise TimeoutError('Timeout aguardando READY:V10.1 (ou recuperacao EEPROM)')

    def armar_movimentos_reais(self):
        """Armamento por ciclo: somente apos operador confirmar posicao e area livre."""
        self._escrever('ARMAR:REAL')
        self._esperar('ARMADO:REAL', 15)

    def armar_aquecimento_apos_inspecao(self):
        """Solicitacao somente apos conferencias fisicas. Arduino tambem bloqueia
        esta operacao ate que firmware seja liberado em configuracao controlada.
        """
        self._escrever('ARMAR:AQUECIMENTO')
        fim = time.monotonic() + 15
        while time.monotonic() < fim:
            linha = self._ler()
            if linha == 'AQUECEDOR:ARMADO_POR_CICLO':
                return
            if linha.startswith('AQUECEDOR:NAO_AUTORIZADO'):
                raise RuntimeError('Aquecedor BLOQUEADO pelo firmware: ' + linha)
        raise TimeoutError('Sem confirmacao de armamento termico')

    @staticmethod
    def registrar_evento_termico(sample, linha):
        """Eventos declarados pelo Arduino; nao inferir passagem real de corrente."""
        if linha.startswith('AQUECEDOR:'):
            sample.eventos_aquecedor.append(linha)
            if linha.startswith('AQUECEDOR:ON_COMANDADO:'):
                sample.aquecimento_comandado = True
                sample.aquecimento_realizado = True  # legado: comando, nao corrente medida
            elif linha.startswith('AQUECEDOR:TEMPO_COMANDO_ON_MS:'):
                sample.tempo_comando_on_ms = int(linha.rsplit(':', 1)[1])
            elif linha.startswith('AQUECEDOR:OFF:'):
                sample.motivo_desligamento_aquecedor = linha.rsplit(':', 1)[1]
        elif linha.startswith('RESFRIAMENTO:'):
            partes = linha.split(':')
            if len(partes) == 3 and partes[1].isdigit():
                sample.medicoes_resfriamento.append({
                    'tempo_s': int(partes[1]),
                    'temp_ntc_c': float(partes[2])
                })

    def centralizar_antes_tara(self):
        """Comando separado: centro fisico confirmado ANTES da tara."""
        self._escrever('CENTRALIZAR')
        self._esperar('SERVO:CENTRO:OK', 25)

    def realizar_tara(self):
        self._escrever('TARA')
        self._esperar('TARA:OK', 30)

    def iniciar_processo(self):
        self._escrever('START')

    def _registrar_vetor(self, linha, sample=None):
        if not linha.startswith('VETOR:'):
            return False
        partes = linha.split(':')
        if len(partes) != 6:
            raise ValueError(f'Vetor malformado: {linha}')
        _, etapa, indice, instante, posicao, peso = partes
        if etapa not in ('P1', 'P2', 'PF', 'P3'):
            raise ValueError(f'Etapa desconhecida: {etapa}')
        i = int(indice)
        if not 1 <= i <= 12:
            raise ValueError(f'Indice de vetor invalido: {linha}')
        if sample is not None:
            vetor = sample.vetores_pesagem[etapa]
            if i != len(vetor) + 1:
                raise ValueError(f'Vetor fora de sequencia: {linha}')
            vetor.append({'indice': i, 'tempo_arduino_ms': int(instante),
                          'posicao_comandada_passos': int(posicao),
                          'peso_g': self._numero(peso, etapa)})
        return True

    def aguardar_p1_e_comando_camera(self, timeout=60):
        fim = time.monotonic() + timeout
        p1 = None
        while time.monotonic() < fim:
            linha = self._ler()
            if self._registrar_vetor(linha, getattr(self, 'amostra_metrologia', None)):
                continue
            if linha.startswith('P1:'):
                p1 = self._numero(linha.split(':', 1)[1], 'P1')
                if p1 <= 0:
                    raise ValueError('P1 deve ser positivo')
            elif linha == 'CMD:ACTIVATE_CAMERA':
                if p1 is None:
                    raise RuntimeError('Camera acionada antes de receber P1')
                return p1
        raise TimeoutError('Tempo esgotado aguardando P1 e camera')

    def enviar_resultado_cnn(self, status):
        comandos = {
            'APROVADA': 'CAM:SUCESSO',
            'REPROVADA': 'CAM:RUIM',
            'INCONCLUSIVA': 'CAM:INCONCLUSIVA'
        }
        if status not in comandos:
            raise ValueError(f'Status CNN nao reconhecido: {status}')
        self._escrever(comandos[status])

    def aguardar_fim_reprovacao(self, status, timeout=30):
        esperado = {
            'REPROVADA': 'FIM:REPROVADA_CNN',
            'INCONCLUSIVA': 'FIM:INCONCLUSIVA_CNN'
        }[status]
        self._esperar(esperado, timeout)

    def obter_serie_pesagens(self, sample, timeout=720, interativo=True):
        fim = time.monotonic() + timeout
        tem_p2 = tem_pf = False
        ultima_vigilancia = time.monotonic()
        retorno_confirmado = False
        while time.monotonic() < fim:
            agora = time.monotonic()
            if sample.aquecimento_comandado and (agora - ultima_vigilancia) >= 1.0:
                self._escrever('PC:ALIVE')
                ultima_vigilancia = agora
            linha = self._ler()
            if not linha:
                continue
            if linha == 'RESFRIAMENTO:APTO_PARA_INSPECAO_FISICA':
                if not interativo:
                    raise RuntimeError('Retorno com calor exige presenca do operador')
                if not retorno_confirmado:
                    print('CAMARA RESFRIADA PELO NTC. Isso nao garante que a lampada')
                    print('e as partes quentes estejam frias. Confira fisicamente')
                    print('estrutura, bandeja, cabos, suporte, percurso e parada de emergencia.')
                    senha = input('Apos verificar, digite RETORNO-RESFRIADO: ').strip()
                    if senha != 'RETORNO-RESFRIADO':
                        raise RuntimeError('Retorno recusado pelo operador')
                    self._escrever('CONFIRMO:RESFRIAMENTO')
                    retorno_confirmado = True
                continue
            if self._registrar_vetor(linha, sample):
                continue
            if linha == 'FIM:1':
                sample.calcular_estatisticas_pesagem()
                if not (tem_p2 and tem_pf and sample.medicoes):
                    raise RuntimeError('Ciclo terminou sem P2, PF ou serie de medicoes')
                sample.calcular_variacao_massa()
                return sample
            if linha.startswith('FIM:'):
                raise RuntimeError(f'Finalizacao inesperada: {linha}')
            if linha.startswith('P2:'):
                sample.peso_referencia = self._numero(linha.split(':', 1)[1], 'P2')
                if sample.peso_referencia <= 0:
                    raise ValueError('P2 invalido')
                tem_p2 = True
            elif linha.startswith('MEDICAO:'):
                partes = linha.split(':')
                if len(partes) != 4:
                    raise ValueError(f'MEDICAO malformada: {linha}')
                seg = int(partes[1])
                peso = self._numero(partes[2], 'peso')
                temp = None if partes[3] == 'ERRO' else self._numero(partes[3], 'temp_ntc')
                if seg < 0 or (sample.medicoes and seg <= sample.medicoes[-1]['tempo_s']):
                    raise ValueError('Tempo de medicoes invalido ou nao crescente')
                if not tem_p2:
                    raise RuntimeError('MEDICAO recebida antes de P2')
                sample.medicoes.append({'tempo_s': seg, 'peso_g': peso, 'temp_ntc_c': temp})
                sample.temperatura_local = temp  # compatibilidade com relatorios legados
                sample.temperatura_camara = temp
            elif linha.startswith('PF:'):
                sample.peso_final = self._numero(linha.split(':', 1)[1], 'PF')
                tem_pf = True
            elif linha == 'P3:ERRO_DIAGNOSTICO':
                sample.diagnostico_p3 = 'ERRO_DIAGNOSTICO'
            elif linha.startswith('P3:'):
                sample.peso_retorno = self._numero(linha.split(':', 1)[1], 'P3')
            elif linha in ('SERVO:PWM_SUSPENSO:SECAGEM', 'SERVO:PWM_REATIVADO:RETORNO'):
                # Registro auditavel no JSON do ciclo aprovado ou interrompido.
                sample.eventos_servo_pwm.append(linha)
            elif linha.startswith('PASSOS:'):
                partes = linha.split(':')
                if len(partes) == 5 and partes[1] in ('SUBIDA', 'DESCIDA'):
                    feitos, total, posicao = map(int, partes[2:])
                    if not (0 <= feitos <= total and 0 <= posicao <= total):
                        raise ValueError(f'Telemetria de motor invalida: {linha}')
                    sample.movimentos.append({
                        'sentido': partes[1], 'passos_executados': feitos,
                        'passos_totais': total, 'posicao_comandada': posicao
                    })
                    sample.movimento_simulado = False
            elif linha.startswith('MOVIMENTO:SIMULADO:'):
                sample.movimento_simulado = True
            elif linha.startswith('MOVIMENTO:REAL:'):
                sample.movimento_simulado = False
            elif linha.startswith('AQUECEDOR:') or linha.startswith('RESFRIAMENTO:'):
                self.registrar_evento_termico(sample, linha)
        raise TimeoutError('Timeout aguardando P2, serie temporal, PF e FIM:1')

    @staticmethod
    def registrar_linha_movimento(sample, linha):
        """Reutilizado tanto durante o ciclo quanto no retorno apos erro."""
        if linha in ('SERVO:PWM_SUSPENSO:SECAGEM', 'SERVO:PWM_REATIVADO:RETORNO'):
            sample.eventos_servo_pwm.append(linha)
        elif linha.startswith('PASSOS:'):
            partes = linha.split(':')
            if len(partes) == 5 and partes[1] in ('SUBIDA', 'DESCIDA'):
                feitos, total, posicao = map(int, partes[2:])
                if not (0 <= feitos <= total and 0 <= posicao <= total):
                    raise ValueError(f'Telemetria de motor invalida: {linha}')
                sample.movimentos.append({
                    'sentido': partes[1], 'passos_executados': feitos,
                    'passos_totais': total, 'posicao_comandada': posicao
                })
                sample.movimento_simulado = False
        elif linha.startswith('MOVIMENTO:SIMULADO:'):
            sample.movimento_simulado = True
        elif linha.startswith('MOVIMENTO:REAL:'):
            sample.movimento_simulado = False

    def notificar_falha_camera(self):
        """Notifica falha de camera/IA para que firmware trate a origem do erro.
        So chamar se CMD:ACTIVATE_CAMERA ja foi recebido.
        """
        self._escrever('CAM:ERRO')
        fim = time.monotonic() + 8
        while time.monotonic() < fim:
            try:
                linha = self._ler()
            except FalhaArduino as exc:
                return exc
        raise TimeoutError('Arduino nao confirmou CAM:ERRO')

    def retorno_apos_erro(self, sample, presencial=False, timeout=150):
        """Retorna APENAS apos falha recuperavel, bandeja suportada e autorizacao local.

        Nao tenta mover se houve reboot, STOP, falha de EEPROM ou posicao incerta.
        Resultado apenas indica movimento COMANDADO, nao homing verificado.
        """
        sample.recuperacao_status = 'NAO_OFERECIDA'
        if not presencial:
            sample.recuperacao_status = 'BLOQUEADA_SEM_OPERADOR'
            return False
        fim = time.monotonic() + 5
        disponivel = False
        while time.monotonic() < fim:
            try:
                linha = self._ler()
            except FalhaArduino as erro:
                sample.eventos_recuperacao.append(erro.linha)
                sample.recuperacao_status = 'OUTRA_FALHA'
                return False
            if linha:
                sample.eventos_recuperacao.append(linha)
            if linha == 'RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL':
                disponivel = True
                break
            if linha.startswith('RECUPERACAO:BLOQUEADA'):
                sample.recuperacao_status = 'BLOQUEADA_PELO_ARDUINO'
                return False
        if not disponivel:
            sample.recuperacao_status = 'SEM_CONFIRMACAO_DE_RETORNO'
            return False
        print('\n[RECUPERACAO] O Arduino informou que o motor ESTA PARADO')
        print('e que o retorno foi disponibilizado nesta sessao, sem reset.')
        print('VERIFIQUE FISICAMENTE: posicao atual, apoio da bandeja,')
        print('percurso sem obstaculos, parada de emergencia independente,')
        print('servo seguro sem PWM e aquecedor desconectado.')
        print('A EEPROM NAO comprova a posicao fisica do motor.')
        resposta = input('Somente apos conferir, digite RETORNAR para tentar regressar: ').strip()
        if resposta != 'RETORNAR':
            sample.recuperacao_status = 'CANCELADA_PELO_OPERADOR'
            return False
        self._escrever('RETORNAR:SUPERVISIONADO')
        fim = time.monotonic() + timeout
        while time.monotonic() < fim:
            try:
                linha = self._ler()
            except FalhaArduino as erro:
                sample.eventos_recuperacao.append(erro.linha)
                sample.recuperacao_status = 'FALHA_DURANTE_RETORNO'
                return False
            if not linha:
                continue
            sample.eventos_recuperacao.append(linha)
            self.registrar_linha_movimento(sample, linha)
            if linha == 'RECUPERACAO:RETORNO_COMANDADO:X_PENDENTE_CONFIRMACAO':
                sample.recuperacao_status = 'RETORNO_COMANDADO_CONFERIR_X'
                return True
            if linha == 'RECUPERACAO:RETORNO_INDISPONIVEL' or linha.startswith('RECUPERACAO:BLOQUEADA'):
                sample.recuperacao_status = 'BLOQUEADA_PELO_ARDUINO'
                return False
        sample.recuperacao_status = 'TIMEOUT_RETORNO_POSICAO_INCERTA'
        return False

    def parar(self):
        if self.serial.is_open:
            self._escrever('STOP')

    def fechar(self):
        if self.serial.is_open:
            self.serial.close()
