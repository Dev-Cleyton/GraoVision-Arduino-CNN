/*
 * TCC - Analise e desidratacao de graos - V10.1 VALIDACAO SEM AQUECIMENTO
 * Base: firmware V10 fornecido pelo usuario, preservado em referencias/.
 * IMPORTANTE: Relés LOW = LIGA. Não conectar cargas de potência.
 * O firmware NÃO aciona o aquecedor sob nenhuma circunstância.
 * Movimentos físicos DESABILITADOS por padrão: testar protocolo antes.
 * Sem sensores de fim de curso/homing e protecao termica confirmados.
 * As posições de 0/90/180 e os 14000 passos vêm do protótipo V10,
 * e NÃO constituem prova de posicionamento seguro.
 *
 * Arduino UNO: HX711 DOUT A1, SCK A0; NTC A2; servo D9;
 * TB6600 DIR D2 STEP D3; relés R1=D7 aquecedor, R2=D6 driver,
 * R3=D8 reserva, R4=D4 reserva. Todos comandados HIGH=OFF.
 *
 * PC -> UNO: TARA, START, CAM:SUCESSO/CAM:OK, CAM:RUIM,
 *             CAM:INCONCLUSIVA, CAM:ERRO, STOP
 * UNO -> PC: READY:V10.1, TARA:OK, P1:<g>, CMD:ACTIVATE_CAMERA,
 *             FIM:REPROVADA_CNN, FIM:INCONCLUSIVA_CNN,
 *             P2:<g>, MEDICAO:<segundos>:<g>:<temp_ntc_c>,
 *             PF:<g>, FIM:1, ERRO:<motivo>
 * NTC A2 = temperatura proxima da celula; nunca umidade dos graos.
 */
#include <Arduino.h>
#include <HX711.h>
#include <Servo.h>
#include <math.h>
#include <string.h>

// CONFIGURACOES DE TESTE - somente mudar apos rever hardware e seguranca.
const bool HABILITAR_MOVIMENTOS_REAIS = false;
const bool HABILITAR_RELE_DRIVER = false; // Se habilitar movimento real, revisar alimentacao/risco.
const unsigned long DURACAO_MONITORAMENTO_MS = 120000UL; // 2 minutos, SEM AQUECIMENTO.
const unsigned long INTERVALO_MEDICAO_MS = 30000UL;
const unsigned long TEMPO_MAX_CNN_MS = 120000UL;
const unsigned long TEMPO_ESTABILIZACAO_MS = 8000UL;
const long PASSOS_SUBIDA = 14000L; // Apenas com movimento real habilitado e percurso validado.
const float FATOR_CONTAGENS_POR_G = 1058.3167f; // provisório: massa não certificada.
const float LIMIAR_CARGA_MIN_G = 1.0f;
const float AMPLITUDE_MAX_G = 0.20f; // provisório; precisa de validacao.

const byte PIN_DOUT = A1, PIN_SCK = A0, PIN_NTC = A2;
const byte PIN_DIR = 2, PIN_STEP = 3, PIN_SERVO = 9;
const byte PIN_RELE_AQUECEDOR = 7, PIN_RELE_DRIVER = 6;
const byte PIN_RELE3 = 8, PIN_RELE4 = 4;
const byte SERVO_CENTRO = 90, SERVO_CAMERA = 0, SERVO_SECAGEM = 180;

HX711 escala;
Servo servoSuporte;

enum Estado {
  AGUARDANDO_TARA, PESA_INICIAL, POSICIONA_CAMERA, ESPERA_CNN,
  RETORNA_CENTRO_CAMERA, ELEVA, POSICIONA_SECAGEM,
  ESTABILIZA_SECAGEM, PESA_REFERENCIA, MONITORA,
  RETORNA_CENTRO_FINAL, DESCE, CONCLUI, CONCLUI_REPROVACAO,
  CONCLUI_INCONCLUSIVA, FALHOU
};
Estado estado = AGUARDANDO_TARA;
bool taraFeita = false;
bool stopSolicitado = false;
int posicaoServo = SERVO_CENTRO;
unsigned long inicioEstado = 0, inicioMonitor = 0, proximaLeitura = 0;
unsigned long ultimoTempoEnviado = 0;
float p1 = NAN, p2 = NAN, pf = NAN;
char linhaSerial[48];
byte indiceSerial = 0;

// Prototipos explicitos: evita declaracoes automáticas antes de enum/struct na IDE.
void todosRelesOff();
void mudarEstado(Estado novo);
void erro(const __FlashStringHelper *mensagem);
void tratarComando(const char *cmd);
void lerSerial();
bool pesar(float &resultado);
float temperaturaNTC();
void enviarPeso(const __FlashStringHelper *nome, float v);
void enviarMedicao(unsigned long tempoS, float peso);
bool moverServo(int destino);
bool moverStepper(bool subir);
void prepararNovoCiclo();
void setup();
void loop();

void todosRelesOff() {
  digitalWrite(PIN_RELE_AQUECEDOR, HIGH);
  digitalWrite(PIN_RELE_DRIVER, HIGH);
  digitalWrite(PIN_RELE3, HIGH);
  digitalWrite(PIN_RELE4, HIGH);
}

void mudarEstado(Estado novo) {
  estado = novo;
  inicioEstado = millis();
}

void erro(const __FlashStringHelper *mensagem) {
  todosRelesOff();
  if (servoSuporte.attached()) servoSuporte.detach();
  Serial.print(F("ERRO:"));
  Serial.println(mensagem);
  stopSolicitado = true;
  mudarEstado(FALHOU);
}

void tratarComando(const char *cmd) {
  if (strcmp(cmd, "STOP") == 0) {
    erro(F("STOP_OPERADOR"));
    return;
  }
  if (estado == AGUARDANDO_TARA && strcmp(cmd, "TARA") == 0) {
    // Tara com timeout por conversao; evita bloqueio indefinido no HX711.
    int64_t somaOffset = 0;
    for (byte i = 0; i < 20; i++) {
      if (!escala.wait_ready_timeout(2500)) {
        Serial.println(F("TARA:ERRO"));
        return;
      }
      somaOffset += escala.read();
    }
    escala.set_offset((long)(somaOffset / 20));
    taraFeita = true;
    stopSolicitado = false;
    Serial.println(F("TARA:OK"));
    return;
  }
  if (estado == AGUARDANDO_TARA && strcmp(cmd, "START") == 0) {
    if (!taraFeita) {
      Serial.println(F("ERRO:TARA_NAO_REALIZADA"));
      return;
    }
    taraFeita = false; // nova tara obrigatoria para proximo ciclo
    p1 = p2 = pf = NAN;
    stopSolicitado = false;
    mudarEstado(PESA_INICIAL);
    return;
  }
  if (estado == ESPERA_CNN) {
    if (strcmp(cmd, "CAM:SUCESSO") == 0 || strcmp(cmd, "CAM:OK") == 0) {
      mudarEstado(RETORNA_CENTRO_CAMERA);
      return;
    }
    if (strcmp(cmd, "CAM:RUIM") == 0) {
      mudarEstado(CONCLUI_REPROVACAO);
      return;
    }
    if (strcmp(cmd, "CAM:INCONCLUSIVA") == 0) {
      mudarEstado(CONCLUI_INCONCLUSIVA);
      return;
    }
    if (strcmp(cmd, "CAM:ERRO") == 0) {
      erro(F("FALHA_CAMERA_CNN"));
      return;
    }
  }
}

void lerSerial() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      linhaSerial[indiceSerial] = '\0';
      if (indiceSerial > 0) tratarComando(linhaSerial);
      indiceSerial = 0;
      if (stopSolicitado) return;
    } else if (indiceSerial < sizeof(linhaSerial) - 1) {
      linhaSerial[indiceSerial++] = c;
    } else {
      indiceSerial = 0; // comando demasiado longo é rejeitado
    }
  }
}

bool pesar(float &resultado) {
  const byte N = 12;
  for (byte tentativa = 0; tentativa < 3; tentativa++) {
    float total = 0.0f, menor = 0.0f, maior = 0.0f;
    for (byte i = 0; i < N; i++) {
      if (!escala.wait_ready_timeout(2500)) return false;
      float v = escala.get_units(1);
      if (isnan(v) || isinf(v)) return false;
      if (i == 0 || v < menor) menor = v;
      if (i == 0 || v > maior) maior = v;
      total += v;
      lerSerial();
      if (stopSolicitado) return false;
      delay(50);
    }
    if (maior - menor <= AMPLITUDE_MAX_G) {
      resultado = total / N;
      return true;
    }
    Serial.print(F("PESO:INSTAVEL:"));
    Serial.println(maior - menor, 4);
  }
  return false;
}

float temperaturaNTC() {
  // Montagem validada: resistor fixo ao 5V; NTC entre A2 e GND.
  long soma = 0;
  for (byte i = 0; i < 16; i++) soma += analogRead(PIN_NTC);
  float adc = soma / 16.0f;
  if (adc <= 0.5f || adc >= 1022.5f) return NAN;
  float resistencia = 10000.0f * adc / (1023.0f - adc);
  float tempK = 1.0f / (1.0f / 298.15f + log(resistencia / 10000.0f) / 3950.0f);
  return tempK - 273.15f;
}

void enviarPeso(const __FlashStringHelper *nome, float v) {
  Serial.print(nome);
  Serial.print(':');
  Serial.println(v, 4); // mais casas diagnosticas nao aumentam exatidao
}

void enviarMedicao(unsigned long tempoS, float peso) {
  Serial.print(F("MEDICAO:"));
  Serial.print(tempoS);
  Serial.print(':');
  Serial.print(peso, 4);
  Serial.print(':');
  float temp = temperaturaNTC();
  if (isnan(temp)) Serial.println(F("ERRO"));
  else Serial.println(temp, 2);
}

bool moverServo(int destino) {
  if (!HABILITAR_MOVIMENTOS_REAIS) {
    posicaoServo = destino;
    Serial.print(F("MOVIMENTO:SIMULADO:SERVO:"));
    Serial.println(destino);
    return true;
  }
  Serial.println(F("MOVIMENTO:REAL:SERVO"));
  if (!servoSuporte.attached()) servoSuporte.attach(PIN_SERVO);
  while (posicaoServo != destino) {
    posicaoServo += destino > posicaoServo ? 1 : -1;
    servoSuporte.write(posicaoServo);
    delay(12);
    lerSerial();
    if (stopSolicitado) return false;
  }
  delay(500);
  // Servo permanece com PWM para evitar perder sustentacao do suporte.
  return true;
}

bool moverStepper(bool subir) {
  if (!HABILITAR_MOVIMENTOS_REAIS) {
    Serial.println(subir ? F("MOVIMENTO:SIMULADO:SUBIDA") : F("MOVIMENTO:SIMULADO:DESCIDA"));
    return true;
  }
  // Ativar somente em montagem revisada e com percurso fisico conhecido.
  Serial.println(F("MOVIMENTO:REAL:MOTOR"));
  if (HABILITAR_RELE_DRIVER) digitalWrite(PIN_RELE_DRIVER, LOW);
  delay(500);
  digitalWrite(PIN_DIR, subir ? HIGH : LOW);
  for (long i = 0; i < PASSOS_SUBIDA; i++) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(300);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(300);
    if ((i % 20) == 0) {
      lerSerial();
      if (stopSolicitado) break;
    }
  }
  digitalWrite(PIN_RELE_DRIVER, HIGH);
  if (stopSolicitado) return false;
  return true;
}

void prepararNovoCiclo() {
  todosRelesOff();
  // Não realinha fisicamente após erro; não há sensor de posição/homing.
  taraFeita = false;
  mudarEstado(AGUARDANDO_TARA);
  Serial.println(F("READY:V10.1"));
}

void setup() {
  // Colocar niveis HIGH antes do modo OUTPUT reduz risco de acionamento no setup,
  // mas NAO elimina pulsos durante reset/boot.
  digitalWrite(PIN_RELE_AQUECEDOR, HIGH);
  digitalWrite(PIN_RELE_DRIVER, HIGH);
  digitalWrite(PIN_RELE3, HIGH);
  digitalWrite(PIN_RELE4, HIGH);
  pinMode(PIN_RELE_AQUECEDOR, OUTPUT);
  pinMode(PIN_RELE_DRIVER, OUTPUT);
  pinMode(PIN_RELE3, OUTPUT);
  pinMode(PIN_RELE4, OUTPUT);
  todosRelesOff();
  digitalWrite(PIN_STEP, LOW);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  Serial.begin(9600);
  escala.begin(PIN_DOUT, PIN_SCK);
  escala.set_scale(FATOR_CONTAGENS_POR_G);
  // Nunca tara automaticamente. O operador deve confirmar prato vazio.
  Serial.println(F("V10.1:VALIDACAO_SEM_AQUECIMENTO"));
  Serial.println(F("MOVIMENTOS:SIMULADOS_POR_PADRAO"));
  Serial.println(F("AVISO:SEM_HOMING_NEM_PROTECAO_TERMICA"));
  Serial.println(F("READY:V10.1"));
}

void loop() {
  lerSerial();
  if (stopSolicitado) return; // após STOP, requer reset físico e nova confirmação de posicionamento.

  switch (estado) {
    case AGUARDANDO_TARA: break;
    case PESA_INICIAL:
      if (!pesar(p1) || p1 < LIMIAR_CARGA_MIN_G) { erro(F("P1_INVALIDO_OU_INSTAVEL")); return; }
      enviarPeso(F("P1"), p1);
      mudarEstado(POSICIONA_CAMERA);
      break;
    case POSICIONA_CAMERA:
      if (!moverServo(SERVO_CAMERA)) return;
      mudarEstado(ESPERA_CNN);
      Serial.println(F("CMD:ACTIVATE_CAMERA"));
      break;
    case ESPERA_CNN:
      if (millis() - inicioEstado > TEMPO_MAX_CNN_MS) erro(F("CNN_TIMEOUT"));
      break;
    case CONCLUI_REPROVACAO:
      if (!moverServo(SERVO_CENTRO)) return;
      todosRelesOff();
      Serial.println(F("FIM:REPROVADA_CNN"));
      prepararNovoCiclo();
      break;
    case CONCLUI_INCONCLUSIVA:
      if (!moverServo(SERVO_CENTRO)) return;
      todosRelesOff();
      Serial.println(F("FIM:INCONCLUSIVA_CNN"));
      prepararNovoCiclo();
      break;
    case RETORNA_CENTRO_CAMERA:
      if (!moverServo(SERVO_CENTRO)) return;
      mudarEstado(ELEVA);
      break;
    case ELEVA:
      if (!moverStepper(true)) return;
      mudarEstado(POSICIONA_SECAGEM);
      break;
    case POSICIONA_SECAGEM:
      if (!moverServo(SERVO_SECAGEM)) return;
      mudarEstado(ESTABILIZA_SECAGEM);
      break;
    case ESTABILIZA_SECAGEM:
      if (millis() - inicioEstado >= TEMPO_ESTABILIZACAO_MS)
        mudarEstado(PESA_REFERENCIA);
      break;
    case PESA_REFERENCIA:
      if (!pesar(p2) || p2 < LIMIAR_CARGA_MIN_G) { erro(F("P2_INVALIDO_OU_INSTAVEL")); return; }
      enviarPeso(F("P2"), p2);
      Serial.println(F("AQUECEDOR:BLOQUEADO_V10_1"));
      inicioMonitor = millis();
      proximaLeitura = inicioMonitor + INTERVALO_MEDICAO_MS;
      ultimoTempoEnviado = 0;
      enviarMedicao(0, p2);
      mudarEstado(MONITORA);
      break;
    case MONITORA:
      if ((long)(millis() - proximaLeitura) >= 0) {
        float leitura;
        if (!pesar(leitura)) { erro(F("PESO_MONITOR_INSTAVEL")); return; }
        // tempo representado pelo relógio real, evitando drift dos atrasos de leitura
        unsigned long tempoS = (millis() - inicioMonitor) / 1000UL;
        if (tempoS <= ultimoTempoEnviado) tempoS = ultimoTempoEnviado + 1UL;
        ultimoTempoEnviado = tempoS;
        enviarMedicao(tempoS, leitura);
        pf = leitura;
        proximaLeitura += INTERVALO_MEDICAO_MS;
      }
      if (millis() - inicioMonitor >= DURACAO_MONITORAMENTO_MS && !isnan(pf)) {
        enviarPeso(F("PF"), pf);
        mudarEstado(RETORNA_CENTRO_FINAL);
      }
      break;
    case RETORNA_CENTRO_FINAL:
      if (!moverServo(SERVO_CENTRO)) return;
      mudarEstado(DESCE);
      break;
    case DESCE:
      if (!moverStepper(false)) return;
      mudarEstado(CONCLUI);
      break;
    case CONCLUI:
      todosRelesOff();
      Serial.println(F("FIM:1"));
      prepararNovoCiclo();
      break;
    case FALHOU: break;
  }
}
