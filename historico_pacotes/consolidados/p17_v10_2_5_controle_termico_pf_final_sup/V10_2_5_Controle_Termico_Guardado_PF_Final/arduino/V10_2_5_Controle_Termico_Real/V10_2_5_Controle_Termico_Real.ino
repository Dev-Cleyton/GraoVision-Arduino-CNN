/*
 * TCC GRAOS - V10.2.5 - ENSAIO INTEGRADO COM DOCUMENTACAO TERMICA
 * Arduino UNO / ATmega328P 16 MHz / serial 9600 baud
 *
 * Base de compatibilidade: firmware V10.1 + Python V10.1B V2.
 * V4/V10: historico de EEPROM; V10.1: tara, P1, CNN, P2, PF, NTC.
 *
 * O QUE ESTA IMPLEMENTADO:
 * - tara manual por ciclo, HX711 com estabilidade e timeout;
 * - servo 0=webcam / 90=centro / 180=posicao de secagem;
 * - 14000 passos de subida e descida, com STOP consultado entre pulsos;
 * - protocolo serial da V10.1B (CAM:SUCESSO / CAM:RUIM / CAM:INCONCLUSIVA);
 * - P2 ANTES do monitoramento, PF, NTC A2, 120 segundos de serie;
 * - EEPROM com journal circular e CRC16, posicao comandada, angulo comandado,
 *   estado do processo e deteccao conservadora de interrupcao de energia;
 * - recuperacao SOMENTE com realinhamento e confirmacao do operador.
 *
 * LIMITACOES IMPORTANTES:
 * - EEPROM armazena posicoes COMANDADAS, nao as confirma mecanicamente.
 * - SEM homing/fim de curso, NTC A2 realocado para a camara; protecao independente ALEGADA, nao auditada,
 *   SEM parada de emergencia certificada: nao e codigo de producao autonomo.
 * - AQUECEDOR BLOQUEADO NESTE BUILD; rotina termica exige aprovacao eletromecanica.
 * - NTC A2 localizado NA CAMARA; rotina de corte termico do software preparada e BLOQUEADA.
 * - Rele IN1 D7 permanece HIGH/OFF por padrao.
 * - Para movimento real e necessario habilitar a compilacao E enviar
 *   ARMAR:REAL explicitamente a cada ciclo (Python V10.1C fornecido).
 * - Nao energizar fisicamente ate aprovar o circuito e os limites termicos.
 * - Relay LOW=ON; durante boot a biblioteca nao elimina pulsos de rele.
 */
#include <Arduino.h>
#include <EEPROM.h>
#include <HX711.h>
#include <Servo.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// ===================== CONFIGURACAO =====================
// PADRAO SEGURO: fluxo completo com MOVIMENTOS SIMULADOS.
// Movimentos reais exigem supervisionamento; rele termico permanece bloqueado
// e percursos precisam estar livres e com parada mecanica acessivel.
const bool PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO = true;
// Se driver for alimentado pelo rele IN2 D6, usar true SOMENTE apos conferir
// retencao mecanica do eixo vertical ao desligar energia.
const bool DRIVER_CONTROLADO_PELO_RELE_D6 = true;
// Fisicamente BLOQUEADO ate conferir tipo, conexao em serie e rating DC
// do corte termico, fusivel, fonte, lampada, carcaca e parada de emergencia.
// Nunca confundir autorizacao por software com certificacao de seguranca.
const bool AQUECEDOR_PERMITIDO_APOS_INSPECAO = false;
// 120 s de COMANDO ON (nao certifica corrente real: requer sensor de corrente).
const unsigned long TEMPO_AQUECIMENTO_MS = 120000UL;
// LIMITES ILUSTRATIVOS, NAO CERTIFICADOS. Devem ser definidos por ensaio
// independente e pelos materiais e protecao antes de qualquer liberacao.
const float TEMP_SOFTWARE_DESLIGAR_C = 50.0f;
const float TEMP_PARA_RETORNO_C = 35.0f;
const unsigned long TEMPO_MAX_RESFRIAMENTO_MS = 300000UL;
const unsigned long INTERVALO_VIGILANCIA_NTC_MS = 200UL;
const unsigned long TIMEOUT_HEARTBEAT_PC_MS = 5000UL;
const unsigned long INTERVALO_LOG_RESFRIAMENTO_MS = 5000UL;
const unsigned long TEMPO_MONITOR_MS = 120000UL;
const unsigned long INTERVALO_PESAGEM_MS = 30000UL;
const unsigned long TEMPO_ESTABILIZACAO_MS = 15000UL; // P2 apos movimento fisico
const unsigned long TIMEOUT_CNN_MS = 120000UL;
const long CURSO_VERTICAL_PASSOS = 14000L;
const float FATOR_HX711_CONTAGENS_POR_G = 1058.3167f; // nao certificado
const float PESO_MINIMO_G = 1.0f;
const float AMPLITUDE_MAXIMA_G = 0.20f; // criterio experimental
// Nao bloquear STEP com gravacoes EEPROM durante deslocamento!
// O estado EXECUTANDO e gravado ANTES; a posicao final APOS o movimento.
// Em desligamento, posicao fisica fica INDETERMINADA e exige RECUPERAR:X.
const uint16_t PASSOS_ENTRE_LOGS_SERIAL = 500;
const uint16_t PERIODO_PASSO_INICIO_US = 1800;  // rampa inicial ~555 pulsos/s
const uint16_t PERIODO_PASSO_CRUZEIRO_US = 750; // ~1333 pulsos/s
const uint16_t PASSOS_RAMPA = 1200;
const uint16_t PULSO_ALTO_US = 200;
const byte INTERVALO_SERVO_MS = 24; // 1 grau a cada ~24 ms
const unsigned long ASSENTAMENTO_SERVO_MS = 1200UL;
const unsigned long ASSENTAMENTO_CENTRO_TARA_MS = 2500UL;
// Desligar pulsos de controle PWM do SERVO somente no repouso em 180 graus,
// antes de medir P2, durante todo o monitoramento e ate registrar PF.
// NUNCA corta os 5V do servo. Somente se a bandeja permanecer apoiada
// mecanicamente sem o torque de retencao do servo.
// Nao alterar os relés do motor: no alto o elevador requer retencao fisica.
const bool SUSPENDER_PWM_SERVO_DURANTE_PESAGEM = true;

const float MAX_AMPLITUDE_TARA_G = 0.20f; // nota: ver ajuste abaixo

// ========================= PINOS ========================
const byte PIN_HX_DT = A1, PIN_HX_SCK = A0, PIN_NTC = A2;
const byte PIN_DIR = 2, PIN_STEP = 3, PIN_SERVO = 9;
const byte RELE_AQUECEDOR = 7, RELE_DRIVER = 6;
const byte RELE_3 = 8, RELE_4 = 4;
const byte ANG_CAMERA = 0, ANG_CENTRO = 90, ANG_SECAGEM = 180;

HX711 balanca;
Servo servoPrato;

enum Estado : uint8_t {
  ESPERA_TARA, PESA_P1, VAI_CAMERA, AGUARDA_CNN,
  VOLTA_CENTRO, SUBIDA, VAI_SECAGEM, ESTABILIZA, PESA_P2,
  MONITORANDO, VOLTA_CENTRO_FINAL, DESCIDA, ENCERRAR_OK,
  ENCERRAR_RUIM, ENCERRAR_INCONCLUSIVA, AGUARDAR_RESFRIAMENTO, ESTADO_FALHA
};
Estado estado = ESPERA_TARA;

// ================= EEPROM COM JOURNAL ==================
// Preserva os primeiros 64 bytes (na V4 e V10 a posicao antiga usava addr 0).
// Cada entrada guarda uma versao inteira: corte durante uma escrita nao deve
// substituir um checkpoint anterior com CRC valido.
const int EEPROM_INICIO = 64;
const uint16_t EEPROM_MAGIC = 0xC71A;
const byte EEPROM_VERSAO = 1;
enum EstadoPersistido : uint8_t { P_LIMPO = 0, P_EXECUTANDO = 1, P_FALHOU = 2 };

struct __attribute__((packed)) Registro {
  uint16_t magic;
  uint8_t versao;
  uint8_t situacao;
  int32_t posicaoPassos;
  uint8_t anguloServo;
  uint8_t etapa;
  uint32_t sequencia;
  uint16_t crc;
};
static_assert(sizeof(Registro) == 16, "Registro EEPROM deve usar 16 bytes");

long posicaoPassos = 0;
byte anguloAtual = ANG_CENTRO;
bool recuperacaoPendente = false;
bool eepromGravacaoFalhou = false;
bool armadoParaMover = false;
bool taraConfirmada = false;
bool servoCentralizadoAntesTara = false;
bool paradaSolicitada = false;
// So permite retorno guiado em falhas de medicao/camera e motor parado
// num extremo comandado, na MESMA sessao energizada e armada.
bool retornoSupervisionadoDisponivel = false;
bool motorEmMovimento = false;
bool aquecimentoArmado = false;
bool aquecedorLigadoPorComando = false;
bool aquecimentoFoiComandado = false;
bool avisoResfriamentoEnviado = false;
bool confirmacaoResfriamentoOperador = false;
unsigned long ultimoHeartbeatPC = 0;
unsigned long instanteOn = 0, instanteOff = 0, instanteVigilancia = 0;
unsigned long inicioResfriamento = 0, ultimaTelemetriaResfriamento = 0;

uint32_t ultimaSequencia = 0;
int ultimoSlot = -1;

unsigned long inicioEstado = 0;
unsigned long inicioSerie = 0;
unsigned long proximaMedicao = 0;
unsigned long ultimoTempoSerie = 0;
float p1 = NAN, p2 = NAN, pf = NAN;
char bufferSerial[48];
byte indiceSerial = 0;

// Prototipos explicitos para Arduino IDE nao gerar declaracoes fora de ordem.
void desligarReles();
void anunciarPronto();
void processarComando(const char *cmd);
void lerComandos();
void falhar(const __FlashStringHelper *motivo);
void falharRecuperavel(const __FlashStringHelper *motivo);
bool persistir(byte situacao);
bool pesar(float &peso);
float lerNTC();
bool moverServo(byte destino);
void suspenderPWMServoSecagem();
bool moverVertical(bool subir);
void trocarEstado(Estado novo);
void finalizarNovoCiclo(const __FlashStringHelper *resultado);
void desligarAquecedor(const __FlashStringHelper *motivo);
bool verificarNTCSeguro();


uint16_t crcRegistro(const Registro &r) {
  const byte *p = reinterpret_cast<const byte *>(&r);
  uint16_t crc = 0xFFFF;
  for (byte i = 0; i < sizeof(Registro) - sizeof(uint16_t); i++) {
    crc ^= ((uint16_t)p[i] << 8);
    for (byte bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

int quantidadeSlots() {
  return (EEPROM.length() - EEPROM_INICIO) / (int)sizeof(Registro);
}

bool lerRegistroValido(int slot, Registro &r) {
  EEPROM.get(EEPROM_INICIO + slot * (int)sizeof(Registro), r);
  return r.magic == EEPROM_MAGIC && r.versao == EEPROM_VERSAO &&
         r.situacao <= P_FALHOU && r.posicaoPassos >= 0 &&
         r.posicaoPassos <= CURSO_VERTICAL_PASSOS && r.anguloServo <= 180 &&
         r.crc == crcRegistro(r);
}

void carregarJournal() {
  int qtd = quantidadeSlots();
  bool encontrado = false;
  Registro maisRecente, candidato;
  for (int s = 0; s < qtd; s++) {
    if (!lerRegistroValido(s, candidato)) continue;
    if (!encontrado || (int32_t)(candidato.sequencia - maisRecente.sequencia) > 0) {
      maisRecente = candidato;
      ultimoSlot = s;
      encontrado = true;
    }
  }
  if (!encontrado) {
    // Sem journal valido: dados V4 legados nao sao tratados como posicao real.
    recuperacaoPendente = true;
    return;
  }
  ultimaSequencia = maisRecente.sequencia;
  posicaoPassos = maisRecente.posicaoPassos;
  anguloAtual = maisRecente.anguloServo;
  recuperacaoPendente = maisRecente.situacao != P_LIMPO ||
                        posicaoPassos != 0 || anguloAtual != ANG_CENTRO;
  Serial.print(F("EEPROM:POS_COMANDADA:"));
  Serial.print(posicaoPassos);
  Serial.print(F(":ANGULO:"));
  Serial.print(anguloAtual);
  Serial.print(F(":SEQ:"));
  Serial.println(ultimaSequencia);
}

bool persistir(byte situacao) {
  if (!PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) return true;
  if (eepromGravacaoFalhou) return false;
  int qtd = quantidadeSlots();
  if (qtd < 2) { eepromGravacaoFalhou = true; return false; }
  Registro r;
  r.magic = EEPROM_MAGIC;
  r.versao = EEPROM_VERSAO;
  r.situacao = situacao;
  r.posicaoPassos = posicaoPassos;
  r.anguloServo = anguloAtual;
  r.etapa = (uint8_t)estado;
  r.sequencia = ultimaSequencia + 1;
  r.crc = crcRegistro(r);
  int slot = (ultimoSlot + 1) % qtd;
  int addr = EEPROM_INICIO + slot * (int)sizeof(Registro);
  const byte *p = reinterpret_cast<const byte *>(&r);

  // Invalidar a assinatura ANTES de atualizar o restante da entrada.
  EEPROM.update(addr, 0);
  EEPROM.update(addr + 1, 0);
  for (byte i = 2; i < sizeof(Registro); i++) EEPROM.update(addr + i, p[i]);
  // Assinatura e escrita por ultimo; so a ultima escrita torna o slot valido.
  EEPROM.update(addr, p[0]);
  EEPROM.update(addr + 1, p[1]);
  Registro teste;
  if (!lerRegistroValido(slot, teste) || teste.sequencia != r.sequencia) {
    eepromGravacaoFalhou = true;
    return false;
  }
  ultimoSlot = slot;
  ultimaSequencia = r.sequencia;
  return true;
}

// Comando de rele != confirmacao de passagem de corrente.
void desligarAquecedor(const __FlashStringHelper *motivo) {
  digitalWrite(RELE_AQUECEDOR, HIGH);
  if (aquecedorLigadoPorComando) {
    aquecedorLigadoPorComando = false;
    instanteOff = millis();
    Serial.print(F("AQUECEDOR:OFF:")); Serial.println(motivo);
    Serial.print(F("AQUECEDOR:TEMPO_COMANDO_ON_MS:"));
    Serial.println(instanteOff - instanteOn);
  }
}

// Falha de NTC ou temperatura software excedida: OFF ANTES de qualquer
// EEPROM, recuperacao, movimentacao ou tentativa de nova pesagem.
bool verificarNTCSeguro() {
  float t = lerNTC();
  if (isnan(t) || isinf(t) || t < 0.0f || t > 95.0f) {
    desligarAquecedor(F("FALHA_NTC"));
    falhar(F("FALHA_NTC_CAMARA"));
    return false;
  }
  if (t >= TEMP_SOFTWARE_DESLIGAR_C) {
    desligarAquecedor(F("LIMITE_SOFTWARE"));
    falhar(F("LIMITE_TEMPERATURA_SOFTWARE"));
    return false;
  }
  return true;
}

// ================== ENTRADAS / SEGURANCA =================
void desligarReles() {
  desligarAquecedor(F("OFF_GERAL"));
  digitalWrite(RELE_DRIVER, HIGH);
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);
}

void desligamentoDeFalha() {
  // NUNCA energizar aquecedor. Se o eixo estiver acima da base,
  // manter D6 ligado para nao REMOVER deliberadamente a retencao do motor.
  // Isso NAO substitui freio/retencao mecanica: falta de energia pode soltar.
  desligarAquecedor(F("FALHA"));
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);
  if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO &&
      DRIVER_CONTROLADO_PELO_RELE_D6 && posicaoPassos > 0) {
    digitalWrite(RELE_DRIVER, LOW);
    Serial.println(F("SEGURANCA:DRIVER_MANTIDO:POSICAO_ACIMA_DE_X"));
  } else {
    digitalWrite(RELE_DRIVER, HIGH);
  }
}

void falhar(const __FlashStringHelper *motivo) {
  desligarAquecedor(F("FALHA")); // priorizar corte do rele
  // A causa do primeiro erro prevalece. Nunca gravar STOP_OPERADOR por cima
  // da causa de PESO_MONITOR_INSTAVEL em uma excecao Python posterior.
  if (estado == ESTADO_FALHA && paradaSolicitada) return;
  Estado etapaAnterior = estado;
  paradaSolicitada = true;
  estado = ESTADO_FALHA;
  desligamentoDeFalha();
  // Nao forcar detach(): perda de torque pode soltar o prato.
  // Se ja esta detach na secagem, segue sem PWM ate retorno supervisionado.
  if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) persistir(P_FALHOU);
  Serial.print(F("ERRO:"));
  Serial.println(motivo);
  Serial.print(F("DIAGNOSTICO_FALHA:ETAPA:")); Serial.print((int)etapaAnterior);
  Serial.print(F(":PASSOS_COMANDADOS:")); Serial.print(posicaoPassos);
  Serial.print(F(":SERVO_ANGULO_COMANDADO:")); Serial.print(anguloAtual);
  Serial.print(F(":MOTOR_EM_MOVIMENTO:")); Serial.println(motorEmMovimento ? 1 : 0);
  if (retornoSupervisionadoDisponivel) {
    Serial.println(F("RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL"));
  } else {
    Serial.println(F("RECUPERACAO:BLOQUEADA:CONFERENCIA_MANUAL"));
  }
}

void falharRecuperavel(const __FlashStringHelper *motivo) {
  // Se STOP foi solicitado durante a leitura HX711, preservar STOP como
  // falha CRITICA. NUNCA transformar uma emergencia em retorno recuperavel.
  if (paradaSolicitada || estado == ESTADO_FALHA) return;
  // Somente erros de diagnostico com hardware PARADO e posicao logica
  // em um extremo conhecido da mesma sessao. Nao permitido apos reboot,
  // falha EEPROM, movimento interrompido ou STOP do operador.
  retornoSupervisionadoDisponivel =
      PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO && armadoParaMover &&
      !recuperacaoPendente && !eepromGravacaoFalhou &&
      !motorEmMovimento && !aquecimentoFoiComandado &&
      !aquecedorLigadoPorComando &&
      (posicaoPassos == 0 || posicaoPassos == CURSO_VERTICAL_PASSOS);
  falhar(motivo);
}

void trocarEstado(Estado novo) {
  estado = novo;
  inicioEstado = millis();
  if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO &&
      !persistir(P_EXECUTANDO)) falhar(F("EEPROM_FALHA"));
}

bool esperarComStop(unsigned long ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < ms) {
    lerComandos();
    if (paradaSolicitada) return false;
    delay(5);
  }
  return true;
}

void anunciarPronto() {
  if (recuperacaoPendente && PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
    Serial.println(F("RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR"));
    return; // NAO enviar READY; Python nao inicia ciclo perigoso.
  }
  Serial.println(F("READY:V10.1")); // Identico ao esperado no Python V10.1B
}

void finalizarNovoCiclo(const __FlashStringHelper *resultado) {
  desligarReles();
  taraConfirmada = false;
  servoCentralizadoAntesTara = false;
  armadoParaMover = false;
  aquecimentoArmado = false;
  aquecimentoFoiComandado = false;
  confirmacaoResfriamentoOperador = false;
  avisoResfriamentoEnviado = false;
  estado = ESPERA_TARA;
  if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
    // Estado limpo SOMENTE quando centro=90 e base=0 foram comandados.
    if (posicaoPassos != 0 || anguloAtual != ANG_CENTRO) {
      falhar(F("POSICAO_FINAL_INCONSISTENTE"));
      return;
    }
    if (!persistir(P_LIMPO)) { falhar(F("EEPROM_FALHA")); return; }
  }
  Serial.println(resultado); // ACK de conclusao so APOS checkpoint limpo
  anunciarPronto();
}

void processarComando(const char *cmd) {
  if (strcmp(cmd, "ESTADO?") == 0) {
    Serial.print(F("STATUS:ETAPA:")); Serial.print((int)estado);
    Serial.print(F(":PASSOS:")); Serial.print(posicaoPassos);
    Serial.print(F(":ANG:")); Serial.print(anguloAtual);
    Serial.print(F(":ARMADO:")); Serial.println(armadoParaMover ? 1 : 0);
    return;
  }
  if (strcmp(cmd, "PC:ALIVE") == 0) {
    if (aquecedorLigadoPorComando) ultimoHeartbeatPC = millis();
    return;
  }
  if (strcmp(cmd, "CONFIRMO:RESFRIAMENTO") == 0) {
    float t = lerNTC();
    if (estado == AGUARDAR_RESFRIAMENTO &&
        !aquecedorLigadoPorComando && !paradaSolicitada &&
        !isnan(t) && t >= 0.0f && t <= TEMP_PARA_RETORNO_C &&
        avisoResfriamentoEnviado) {
      confirmacaoResfriamentoOperador = true;
      Serial.println(F("RESFRIAMENTO:CONFIRMACAO_PRESENCIAL_RECEBIDA"));
    } else {
      Serial.println(F("RESFRIAMENTO:CONFIRMACAO_RECUSADA"));
    }
    return;
  }
  if (strcmp(cmd, "RETORNAR:SUPERVISIONADO") == 0) {
    if (estado != ESTADO_FALHA || !paradaSolicitada ||
        !retornoSupervisionadoDisponivel || !armadoParaMover ||
        recuperacaoPendente || motorEmMovimento || eepromGravacaoFalhou ||
        (posicaoPassos != 0 && posicaoPassos != CURSO_VERTICAL_PASSOS)) {
      Serial.println(F("RECUPERACAO:RETORNO_INDISPONIVEL"));
      return;
    }
    float tRetorno = lerNTC();
    if (isnan(tRetorno) || tRetorno > TEMP_PARA_RETORNO_C ||
        aquecedorLigadoPorComando || aquecimentoFoiComandado) {
      Serial.println(F("RECUPERACAO:BLOQUEADA:RESFRIAMENTO_OU_AQUECIMENTO"));
      return;
    }
    retornoSupervisionadoDisponivel = false; // uma tentativa, nao repetivel
    paradaSolicitada = false;
    Serial.println(F("RECUPERACAO:RETORNO_INICIADO"));
    // Movimento com o mesmo controle de rampas da R3. Em recuperacao apos falha, so agir se suporte
    // e percurso forem inspecionados com o aquecedor fisicamente OFF.
    if (!moverServo(ANG_CENTRO)) return;
    if (posicaoPassos == CURSO_VERTICAL_PASSOS) {
      if (!moverVertical(false)) return;
    }
    if (paradaSolicitada || posicaoPassos != 0 || anguloAtual != ANG_CENTRO) {
      falhar(F("RETORNO_POSICAO_LOGICA_INVALIDA")); return;
    }
    // O retorno e apenas COMANDADO. Nao libera automaticamente para outro
    // ciclo, pois nao ha fim de curso ou sensor de angulo fisico.
    estado = ESPERA_TARA;
    recuperacaoPendente = true;
    armadoParaMover = false;
    taraConfirmada = false;
    servoCentralizadoAntesTara = false;
    if (!persistir(P_FALHOU)) {
      falhar(F("EEPROM_FALHA_NO_RETORNO")); return;
    }
    Serial.println(F("RECUPERACAO:RETORNO_COMANDADO:X_PENDENTE_CONFIRMACAO"));
    anunciarPronto();
    return;
  }
  if (strcmp(cmd, "STOP") == 0) {
    falhar(F("STOP_OPERADOR"));
    return;
  }
  // So pode ser usado com equipamento PARADO, aquecedor desconectado,
  // amostra removida, prato verificado fisicamente em X inferior + centro.
  // Nao movimenta servo ou stepper: configura a origem LOGICA confirmada.
  if (strcmp(cmd, "RECUPERAR:X") == 0) {
    if (!PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
      Serial.println(F("ERRO:RECUPERACAO_NAO_NECESSARIA_EM_SIMULACAO"));
      return;
    }
    if (estado != ESPERA_TARA || armadoParaMover) {
      Serial.println(F("ERRO:RECUPERACAO_FORA_DO_ESTADO_SEGURO"));
      return;
    }
    // Manual, nao aceitar comando sem preparacao fisica realizada.
    posicaoPassos = 0;
    anguloAtual = ANG_CENTRO;
    recuperacaoPendente = false;
    if (!persistir(P_LIMPO)) { falhar(F("EEPROM_FALHA")); return; }
    Serial.println(F("RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL"));
    anunciarPronto();
    return;
  }
  if (strcmp(cmd, "CENTRALIZAR") == 0) {
    if (estado != ESPERA_TARA || paradaSolicitada || recuperacaoPendente) {
      Serial.println(F("ERRO:CENTRALIZAR_NAO_AUTORIZADO")); return;
    }
    if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO && !armadoParaMover) {
      Serial.println(F("ERRO:REAL_NAO_ARMADO")); return;
    }
    // Registra processo em andamento ANTES de acionar o servo.
    if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO && !persistir(P_EXECUTANDO)) {
      falhar(F("EEPROM_FALHA")); return;
    }
    if (!moverServo(ANG_CENTRO)) return;
    servoCentralizadoAntesTara = true;
    Serial.println(F("SERVO:CENTRO:OK"));
    return;
  }
  if (strcmp(cmd, "ARMAR:AQUECIMENTO") == 0) {
    if (!AQUECEDOR_PERMITIDO_APOS_INSPECAO ||
        !PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO ||
        !armadoParaMover || recuperacaoPendente || paradaSolicitada ||
        estado != ESPERA_TARA) {
      Serial.println(F("AQUECEDOR:NAO_AUTORIZADO_NESTA_COMPILACAO"));
      return;
    }
    float t = lerNTC();
    if (isnan(t) || t < 0.0f || t >= TEMP_SOFTWARE_DESLIGAR_C) {
      Serial.println(F("AQUECEDOR:NAO_AUTORIZADO_NTC"));
      return;
    }
    aquecimentoArmado = true;
    Serial.println(F("AQUECEDOR:ARMADO_POR_CICLO"));
    return;
  }
  if (strcmp(cmd, "ARMAR:REAL") == 0) {
    if (!PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
      Serial.println(F("ERRO:MOVIMENTO_REAL_NAO_COMPILADO")); return;
    }
    if (recuperacaoPendente || paradaSolicitada || estado != ESPERA_TARA) {
      Serial.println(F("ERRO:MOVIMENTO_NAO_PODE_SER_ARMADO")); return;
    }
    // Exige confirmacao presencial pelo programa Python; sem autoarmar no boot.
    armadoParaMover = true;
    servoCentralizadoAntesTara = false;
    Serial.println(F("ARMADO:REAL"));
    return;
  }
  if (recuperacaoPendente && PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
    Serial.println(F("ERRO:RECUPERACAO_PENDENTE")); return;
  }
  if (paradaSolicitada) return;
  if (estado == ESPERA_TARA && strcmp(cmd, "TARA") == 0) {
    if (!servoCentralizadoAntesTara) {
      Serial.println(F("ERRO:SERVO_NAO_CENTRALIZADO")); return;
    }
    if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO && !armadoParaMover) {
      Serial.println(F("ERRO:REAL_NAO_ARMADO")); return;
    }
    // Centro ja concluiu seu assentamento. Agora conferir estabilidade
    // do HX711 com a bandeja VAZIA antes de aplicar o offset.
    Serial.println(F("TARA:VERIFICANDO_ESTABILIDADE"));
    bool estavel = false;
    for (byte tentativa = 0; tentativa < 3 && !estavel; tentativa++) {
      int64_t soma = 0;
      long minRaw = 0, maxRaw = 0;
      for (byte i = 0; i < 20; i++) {
        if (!balanca.wait_ready_timeout(2500)) {
          falhar(F("TARA_HX711_TIMEOUT")); return;
        }
        long raw = balanca.read();
        if (i == 0 || raw < minRaw) minRaw = raw;
        if (i == 0 || raw > maxRaw) maxRaw = raw;
        soma += raw;
        lerComandos();
        if (paradaSolicitada) return;
        if (!esperarComStop(55)) return;
      }
      float amplitudeG = ((float)(maxRaw - minRaw)) / FATOR_HX711_CONTAGENS_POR_G;
      if (amplitudeG <= MAX_AMPLITUDE_TARA_G) {
        balanca.set_offset((long)(soma / 20));
        estavel = true;
      } else {
        Serial.print(F("TARA:INSTAVEL:")); Serial.println(amplitudeG, 4);
        if (!esperarComStop(1200)) return;
      }
    }
    if (!estavel) { falhar(F("TARA_INSTAVEL")); return; }
    taraConfirmada = true;
    Serial.println(F("TARA:OK"));
    return;
  }
  if (estado == ESPERA_TARA && strcmp(cmd, "START") == 0) {
    if (!taraConfirmada) { Serial.println(F("ERRO:TARA_NAO_REALIZADA")); return; }
    if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO && !armadoParaMover) {
      Serial.println(F("ERRO:REAL_NAO_ARMADO")); return;
    }
    taraConfirmada = false;
    p1 = p2 = pf = NAN;
    aquecimentoFoiComandado = false;
    confirmacaoResfriamentoOperador = false;
    avisoResfriamentoEnviado = false;
    trocarEstado(PESA_P1);
    return;
  }
  if (estado == AGUARDA_CNN) {
    if (!strcmp(cmd, "CAM:SUCESSO") || !strcmp(cmd, "CAM:OK")) {
      trocarEstado(VOLTA_CENTRO); return;
    }
    if (!strcmp(cmd, "CAM:RUIM")) { trocarEstado(ENCERRAR_RUIM); return; }
    if (!strcmp(cmd, "CAM:INCONCLUSIVA")) { trocarEstado(ENCERRAR_INCONCLUSIVA); return; }
    if (!strcmp(cmd, "CAM:ERRO")) { falharRecuperavel(F("FALHA_CAMERA_CNN")); return; }
  }
}

void lerComandos() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      bufferSerial[indiceSerial] = '\0';
      if (indiceSerial > 0) processarComando(bufferSerial);
      indiceSerial = 0;
      if (paradaSolicitada) return;
    } else if (indiceSerial < sizeof(bufferSerial) - 1) {
      bufferSerial[indiceSerial++] = c;
    } else {
      indiceSerial = 0; // comando extenso e descartado
    }
  }
}

// ================= PESAGEM E TEMPERATURA =================
bool pesar(float &resultado) {
  const byte N = 12;
  for (byte tentativa = 0; tentativa < 3; tentativa++) {
    float soma = 0, minimo = 0, maximo = 0;
    for (byte i = 0; i < N; i++) {
      if (!balanca.wait_ready_timeout(2500)) return false;
      float peso = balanca.get_units(1);
      if (isnan(peso) || isinf(peso)) return false;
      if (i == 0 || peso < minimo) minimo = peso;
      if (i == 0 || peso > maximo) maximo = peso;
      soma += peso;
      lerComandos();
      if (paradaSolicitada) return false;
      delay(50);
    }
    if (maximo - minimo <= AMPLITUDE_MAXIMA_G) {
      resultado = soma / N;
      return true;
    }
    Serial.print(F("PESO:INSTAVEL:"));
    Serial.println(maximo - minimo, 4);
  }
  return false;
}

float lerNTC() {
  // Resistor de 10k ao 5V, NTC ao GND. A2 mede a CAMARA conforme
  // instalacao atual informada pelo operador (nao calibrada contra referencia).
  long soma = 0;
  for (byte i = 0; i < 16; i++) soma += analogRead(PIN_NTC);
  float adc = soma / 16.0f;
  if (adc <= 0.5f || adc >= 1022.5f) return NAN;
  float resistencia = 10000.0f * adc / (1023.0f - adc);
  float kelvin = 1.0f / (1.0f / 298.15f + log(resistencia / 10000.0f) / 3950.0f);
  return kelvin - 273.15f;
}

void emitirPeso(const __FlashStringHelper *nome, float peso) {
  Serial.print(nome); Serial.print(':'); Serial.println(peso, 4);
}

void emitirMedicao(unsigned long segundos, float peso) {
  Serial.print(F("MEDICAO:")); Serial.print(segundos);
  Serial.print(':'); Serial.print(peso, 4);
  Serial.print(':');
  float t = lerNTC();
  if (isnan(t)) Serial.println(F("ERRO"));
  else Serial.println(t, 2);
}

// ===================== MOVIMENTOS ======================
bool moverServo(byte destino) {
  if (!PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
    anguloAtual = destino;
    Serial.print(F("MOVIMENTO:SIMULADO:SERVO:")); Serial.println(destino);
    return true;
  }
  if (!armadoParaMover || recuperacaoPendente) {
    falhar(F("SERVO_NAO_AUTORIZADO")); return false;
  }
  Serial.println(F("MOVIMENTO:REAL:SERVO"));
  // Nunca conectar servo automaticamente no boot. O operador confere a area.
  if (!servoPrato.attached()) {
    // Na retomada, reenvia primeiro o angulo comandado ja conhecido.
    // Sem sensor de angulo, nao sabemos se o eixo cedeu enquanto desligado.
    servoPrato.write(anguloAtual);
    servoPrato.attach(PIN_SERVO);
    if (anguloAtual == ANG_SECAGEM) {
      Serial.println(F("SERVO:PWM_REATIVADO:RETORNO"));
    }
    if (!esperarComStop(500)) return false;
  }
  // A EEPROM JA contem P_EXECUTANDO gravado ANTES de iniciar.
  // Nenhuma gravação bloqueante ocorre no deslocamento angular.
  while (anguloAtual != destino) {
    anguloAtual += (destino > anguloAtual) ? 1 : -1;
    servoPrato.write(anguloAtual);
    if (!esperarComStop(INTERVALO_SERVO_MS)) return false;
  }
  // Registra destino comandado somente depois de acabar o giro.
  if (!persistir(P_EXECUTANDO)) { falhar(F("EEPROM_FALHA")); return false; }
  unsigned long pausa = destino == ANG_CENTRO && estado == ESPERA_TARA
                          ? ASSENTAMENTO_CENTRO_TARA_MS : ASSENTAMENTO_SERVO_MS;
  if (!esperarComStop(pausa)) return false;
  if (destino == ANG_CENTRO && estado == ESPERA_TARA) {
    Serial.println(F("SERVO:ACOMODADO:PRONTO_PARA_TARA"));
  }
  return true;
}

// Interrompe somente os pulsos PWM do servo apos a bandeja acomodar.
// Nao desenergiza motor de passo, nem relés; aquecedor segue BLOQUEADO.
// O firmware NAO pode verificar se existe sustentacao mecanica independente.
void suspenderPWMServoSecagem() {
  if (!PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO ||
      !SUSPENDER_PWM_SERVO_DURANTE_PESAGEM) return;
  if (anguloAtual != ANG_SECAGEM) {
    falhar(F("SERVO_FORA_DA_SECAGEM_PARA_PWM_OFF"));
    return;
  }
  if (servoPrato.attached()) {
    servoPrato.detach();
    Serial.println(F("SERVO:PWM_SUSPENSO:SECAGEM"));
  }
}

unsigned int periodoPassoUs(long progresso) {
  long restantes = CURSO_VERTICAL_PASSOS - progresso;
  long distanciaRampa = progresso < restantes ? progresso : restantes;
  if (distanciaRampa >= PASSOS_RAMPA) return PERIODO_PASSO_CRUZEIRO_US;
  unsigned long extra = (unsigned long)(PASSOS_RAMPA - distanciaRampa) *
                        (PERIODO_PASSO_INICIO_US - PERIODO_PASSO_CRUZEIRO_US) /
                        PASSOS_RAMPA;
  return (unsigned int)(PERIODO_PASSO_CRUZEIRO_US + extra);
}

bool moverVertical(bool subir) {
  if (!PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) {
    Serial.println(subir ? F("MOVIMENTO:SIMULADO:SUBIDA") : F("MOVIMENTO:SIMULADO:DESCIDA"));
    return true;
  }
  if (!armadoParaMover || recuperacaoPendente) { falhar(F("MOTOR_NAO_AUTORIZADO")); return false; }
  long destino = subir ? CURSO_VERTICAL_PASSOS : 0;
  if ((subir && posicaoPassos != 0) || (!subir && posicaoPassos != CURSO_VERTICAL_PASSOS)) {
    falhar(F("POSICAO_LOGICA_MOTOR_INVALIDA")); return false;
  }
  Serial.println(F("MOVIMENTO:REAL:MOTOR"));
  if (!persistir(P_EXECUTANDO)) { falhar(F("EEPROM_FALHA")); return false; }
  if (DRIVER_CONTROLADO_PELO_RELE_D6) digitalWrite(RELE_DRIVER, LOW);
  if (!esperarComStop(400)) return false;
  digitalWrite(PIN_DIR, subir ? HIGH : LOW);
  long contadorDesdeCheckpoint = 0;
  // PASSOS:SENTIDO:REALIZADOS_NO_MOVIMENTO:TOTAL:POSICAO_COMANDADA
  // Telemetria do numero de pulsos emitidos; nao confirma deslocamento real.
  Serial.print(F("PASSOS:")); Serial.print(subir ? F("SUBIDA:") : F("DESCIDA:"));
  Serial.print(0); Serial.print(':'); Serial.print(CURSO_VERTICAL_PASSOS);
  Serial.print(':'); Serial.println(posicaoPassos);
  motorEmMovimento = true;
  while (posicaoPassos != destino) {
    unsigned int periodo = periodoPassoUs(contadorDesdeCheckpoint);
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(PULSO_ALTO_US);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(periodo - PULSO_ALTO_US);
    posicaoPassos += subir ? 1 : -1;
    contadorDesdeCheckpoint++;
    if ((contadorDesdeCheckpoint % PASSOS_ENTRE_LOGS_SERIAL) == 0 || posicaoPassos == destino) {
      Serial.print(F("PASSOS:")); Serial.print(subir ? F("SUBIDA:") : F("DESCIDA:"));
      Serial.print(contadorDesdeCheckpoint); Serial.print(':'); Serial.print(CURSO_VERTICAL_PASSOS);
      Serial.print(':'); Serial.println(posicaoPassos);
    }
    if ((contadorDesdeCheckpoint % 20) == 0) {
      lerComandos();
      if (paradaSolicitada) return false;
    }
  }
  motorEmMovimento = false;
  // Grava a posicao final JA COM O MOTOR PARADO; sem trancos por EEPROM.
  if (!persistir(P_EXECUTANDO)) { falhar(F("EEPROM_FALHA")); return false; }
  Serial.print(F("POS_ATUADOR:")); Serial.println(posicaoPassos);
  // Manter driver alimentado no topo se houver necessidade de sustentacao.
  // Somente liberar rele de driver na base, apos o retorno concluido.
  if (!subir && DRIVER_CONTROLADO_PELO_RELE_D6) digitalWrite(RELE_DRIVER, HIGH);
  return true;
}

// ======================= ARDUINO ========================
void setup() {
  // HIGH antes de OUTPUT reduz risco no setup, mas nao impede pulsos no boot.
  digitalWrite(RELE_AQUECEDOR, HIGH);
  digitalWrite(RELE_DRIVER, HIGH);
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);
  pinMode(RELE_AQUECEDOR, OUTPUT);
  pinMode(RELE_DRIVER, OUTPUT);
  pinMode(RELE_3, OUTPUT);
  pinMode(RELE_4, OUTPUT);
  desligarReles();
  digitalWrite(PIN_STEP, LOW);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  Serial.begin(9600);
  balanca.begin(PIN_HX_DT, PIN_HX_SCK);
  balanca.set_scale(FATOR_HX711_CONTAGENS_POR_G);
  // SEM tara, movimento, servo.attach ou acionamento de rele no boot.
  Serial.println(F("V10.2.5:INTEGRADO_MASSA_NTC_CAMARA_SEM_AQUECIMENTO"));
  Serial.println(PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO ?
                 F("MOVIMENTOS:REAIS_EXIGEM_ARMAR") : F("MOVIMENTOS:SIMULADOS_POR_PADRAO"));
  Serial.println(AQUECEDOR_PERMITIDO_APOS_INSPECAO ?
     F("AQUECEDOR:EXIGE_ARMAMENTO_MANUAL_E_CORTE_TERMICO") :
     F("AQUECEDOR:BLOQUEADO_V10_2_5"));
  Serial.println(AQUECEDOR_PERMITIDO_APOS_INSPECAO ?
    F("AQUECEDOR:CONTROLE_PREPARADO_EXIGE_ARMAR") :
    F("AQUECEDOR:TRAVADO_FISICAMENTE_POR_FIRMWARE"));
  Serial.println(F("NTC:LOCALIZACAO:CAMARA:A2"));
  Serial.println(F("TERMOSEGURANCA:CORTE_INDEPENDENTE_REFERIDO_POR_OPERADOR_NAO_AUDITADO"));
  Serial.println(F("AVISO:SEM_HOMING_TERMOSEGURANCA_NAO_AUDITADA"));
  Serial.println(F("PASSOS:TELEMETRIA_A_CADA_500"));
  Serial.println(F("STEP:RAMPAS_SUAVES_EEPROM_FORA_DO_MOVIMENTO"));
  Serial.println(F("FALHAS:RETORNO_SOMENTE_COM_CONFIRMACAO_PRESENCIAL"));
  Serial.println(SUSPENDER_PWM_SERVO_DURANTE_PESAGEM ?
                 F("SERVO:PWM_OFF_DURANTE_PESAGEM_HABILITADO") :
                 F("SERVO:PWM_OFF_DURANTE_PESAGEM_DESABILITADO"));
  if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO) carregarJournal();
  anunciarPronto();
}

void loop() {
  lerComandos();
  if (paradaSolicitada || recuperacaoPendente) return;
  switch (estado) {
    case ESPERA_TARA: break;
    case PESA_P1:
      if (!pesar(p1) || p1 < PESO_MINIMO_G) { falharRecuperavel(F("P1_INVALIDO_OU_INSTAVEL")); return; }
      emitirPeso(F("P1"), p1);
      trocarEstado(VAI_CAMERA);
      break;
    case VAI_CAMERA:
      if (!moverServo(ANG_CAMERA)) return;
      trocarEstado(AGUARDA_CNN);
      if (!paradaSolicitada) Serial.println(F("CMD:ACTIVATE_CAMERA"));
      break;
    case AGUARDA_CNN:
      if (millis() - inicioEstado > TIMEOUT_CNN_MS) falharRecuperavel(F("CNN_TIMEOUT"));
      break;
    case ENCERRAR_RUIM:
      if (!moverServo(ANG_CENTRO)) return;
      finalizarNovoCiclo(F("FIM:REPROVADA_CNN"));
      break;
    case ENCERRAR_INCONCLUSIVA:
      if (!moverServo(ANG_CENTRO)) return;
      finalizarNovoCiclo(F("FIM:INCONCLUSIVA_CNN"));
      break;
    case VOLTA_CENTRO:
      if (!moverServo(ANG_CENTRO)) return;
      trocarEstado(SUBIDA);
      break;
    case SUBIDA:
      if (!moverVertical(true)) return;
      trocarEstado(VAI_SECAGEM);
      break;
    case VAI_SECAGEM:
      if (!moverServo(ANG_SECAGEM)) return;
      // R3 ja assentou o servo; agora silencia o PWM ANTES da pesagem P2.
      suspenderPWMServoSecagem();
      if (paradaSolicitada) return;
      trocarEstado(ESTABILIZA);
      break;
    case ESTABILIZA:
      if (millis() - inicioEstado >= TEMPO_ESTABILIZACAO_MS) trocarEstado(PESA_P2);
      break;
    case PESA_P2: {
      if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO &&
          SUSPENDER_PWM_SERVO_DURANTE_PESAGEM && servoPrato.attached()) {
        falhar(F("PWM_SERVO_ATIVO_DURANTE_PESAGEM")); return;
      }
      if (!pesar(p2) || p2 < PESO_MINIMO_G) {
        falharRecuperavel(F("P2_INVALIDO_OU_INSTAVEL")); return;
      }
      emitirPeso(F("P2"), p2);
      // P2 e obtido ANTES do primeiro comando de aquecimento.
      if (aquecimentoArmado && AQUECEDOR_PERMITIDO_APOS_INSPECAO) {
        if (!verificarNTCSeguro()) return;
        if (paradaSolicitada) return;
        // Inicio ON correlacionado ao cronometro independente do monitor.
        instanteOn = millis();
        ultimoHeartbeatPC = instanteOn;
        aquecimentoFoiComandado = true;
        aquecedorLigadoPorComando = true;
        inicioSerie = instanteOn;
        digitalWrite(RELE_AQUECEDOR, LOW);
        Serial.println(F("AQUECEDOR:ON_COMANDADO:0"));
      } else {
        inicioSerie = millis();
        Serial.println(F("AQUECEDOR:DESLIGADO:MONITORAMENTO_SEM_CALOR"));
      }
      proximaMedicao = inicioSerie + INTERVALO_PESAGEM_MS;
      ultimoTempoSerie = 0;
      instanteVigilancia = millis();
      emitirMedicao(0, p2);
      trocarEstado(MONITORANDO);
      break;
    }
    case MONITORANDO: {
      if (PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO &&
          SUSPENDER_PWM_SERVO_DURANTE_PESAGEM && servoPrato.attached()) {
        falhar(F("PWM_SERVO_REATIVADO_DURANTE_MONITOR")); return;
      }
      unsigned long agora = millis();
      unsigned long elapsed = agora - inicioSerie;
      // Tempo e temperatura sao MONITORADOS ANTES de iniciar qualquer
      // operacao de HX711 que possa bloquear por alguns segundos.
      if (aquecedorLigadoPorComando) {
        if (agora - ultimoHeartbeatPC > TIMEOUT_HEARTBEAT_PC_MS) {
          desligarAquecedor(F("SUPERVISAO_PC_PERDIDA"));
          falhar(F("TIMEOUT_SUPERVISAO_PC")); return;
        }
        if (agora - instanteOn >= TEMPO_AQUECIMENTO_MS) {
          desligarAquecedor(F("TEMPO_120S"));
        } else if (agora - instanteVigilancia >= INTERVALO_VIGILANCIA_NTC_MS) {
          instanteVigilancia = agora;
          if (!verificarNTCSeguro()) return;
        }
      }
      // As leituras intermediarias nao substituem a pesagem final.
      if (elapsed < TEMPO_MONITOR_MS && (long)(agora - proximaMedicao) >= 0) {
        float leitura;
        if (!pesar(leitura)) { falharRecuperavel(F("PESO_MONITOR_INSTAVEL")); return; }
        unsigned long seg = (millis() - inicioSerie) / 1000UL;
        if (seg <= ultimoTempoSerie) seg = ultimoTempoSerie + 1;
        ultimoTempoSerie = seg;
        emitirMedicao(seg, leitura);
        proximaMedicao += INTERVALO_PESAGEM_MS;
        return;
      }
      if (millis() - inicioSerie >= TEMPO_MONITOR_MS) {
        // PF e medido REALMENTE apos os 120s, nunca copiado de t=90s.
        desligarAquecedor(F("FIM_MONITORAMENTO"));
        if (!pesar(pf) || pf < PESO_MINIMO_G) {
          falharRecuperavel(F("PF_INVALIDO_OU_INSTAVEL")); return;
        }
        unsigned long instantePF = (millis() - inicioSerie) / 1000UL;
        if (instantePF <= ultimoTempoSerie) instantePF = ultimoTempoSerie + 1;
        emitirMedicao(instantePF, pf);
        emitirPeso(F("PF"), pf);
        if (aquecimentoFoiComandado) {
          inicioResfriamento = millis();
          ultimaTelemetriaResfriamento = 0;
          avisoResfriamentoEnviado = false;
          confirmacaoResfriamentoOperador = false;
          trocarEstado(AGUARDAR_RESFRIAMENTO);
        } else {
          trocarEstado(VOLTA_CENTRO_FINAL);
        }
      }
      break;
    }
    case AGUARDAR_RESFRIAMENTO: {
      // Sem reposicionamento da amostra quente. NTC mede apenas um ponto:
      // retorno so apos inspecao fisica adicional dos materiais/atuadores.
      desligarAquecedor(F("RESFRIAMENTO"));
      unsigned long agora = millis();
      float t = lerNTC();
      if (isnan(t) || isinf(t) || t < 0.0f || t > 95.0f) {
        falhar(F("NTC_INVALIDO_RESFRIAMENTO")); return;
      }
      if (agora - ultimaTelemetriaResfriamento >= INTERVALO_LOG_RESFRIAMENTO_MS) {
        ultimaTelemetriaResfriamento = agora;
        Serial.print(F("RESFRIAMENTO:"));
        Serial.print((agora - inicioResfriamento)/1000UL);
        Serial.print(':'); Serial.println(t, 2);
      }
      if (t <= TEMP_PARA_RETORNO_C) {
        if (!avisoResfriamentoEnviado) {
          avisoResfriamentoEnviado = true;
          Serial.println(F("RESFRIAMENTO:APTO_PARA_INSPECAO_FISICA"));
        }
        if (confirmacaoResfriamentoOperador) {
          trocarEstado(VOLTA_CENTRO_FINAL);
          break;
        }
      } else {
        // Se aqueceu novamente apos liberar, cancela permissao anterior.
        avisoResfriamentoEnviado = false;
        confirmacaoResfriamentoOperador = false;
      }
      if (agora - inicioResfriamento >= TEMPO_MAX_RESFRIAMENTO_MS) {
        falhar(F("TIMEOUT_RESFRIAMENTO_SEM_RETORNO"));
      }
      break;
    }
    case VOLTA_CENTRO_FINAL:
      if (!moverServo(ANG_CENTRO)) return;
      trocarEstado(DESCIDA);
      break;
    case DESCIDA:
      if (!moverVertical(false)) return;
      trocarEstado(ENCERRAR_OK);
      break;
    case ENCERRAR_OK:
      desligarReles();
      finalizarNovoCiclo(F("FIM:1"));
      break;
    case ESTADO_FALHA: break;
  }
}
