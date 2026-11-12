/*
 * TESTE_HX711_SERVO_MOTOR_V10.ino
 * TCC Arduino_Grao_CNN - ensaio integrado de pesagem e atuadores.
 *
 * HARDWARE TESTADO NOS ENSAIOS ANTERIORES:
 *   HX711: DT=A1, SCK=A0 | NTC=A2 | Servo PWM=D9
 *   Driver do motor de passo: DIR=D2, STEP=D3
 *   Fator provisório: +1058.3167 contagens/g
 *   Bibliotecas Arduino: HX711 (bogde) e Servo (nativa).
 *
 * FASES (uma unica tara, 5 pontos por fase, 24 leituras/ponto):
 * A: vazio, driver sem alimentacao, servo SEM PWM
 * B: carga fixa ~36 g, driver desligado, servo SEM PWM
 * C: servo PWM em 90 graus, driver desligado
 * D: servo realiza 90 -> 100 -> 90, leitura APOS estabilizacao
 * E: servo.detach(), driver desligado
 * F: driver energizado, motor PARADO, servo.detach()
 * G: servo PWM em 90 graus, driver energizado, motor PARADO
 * H: servo.detach(), driver energizado, motor PARADO
 * I: 12 pulsos ida + 12 volta, servo.detach(), leitura APOS movimento
 * J: driver desligado, servo.detach(), carga mantida
 * K: carga retirada, driver desligado, servo.detach()
 *
 * SEGURANCA OBRIGATORIA:
 * - Aquecedor fisicamente SEM ALIMENTACAO DE POTENCIA.
 * - D6/D11 nao sao configurados: relays ativos LOW podem ser acionados.
 * - Fonte do driver ligada/desligada APENAS por chave apropriada.
 * - NUNCA remover/conectar cabos do motor com o driver energizado.
 * - Fonte 5 V do servo deve suportar a corrente; GND em comum.
 * - Ao dar attach(), servo pode se deslocar ate 90 graus. Confirme espaco.
 * - Verificar limites e folga da plataforma ANTES de autorizar 12 pulsos.
 * - ESTE TESTE NAO LE SENSORES DE FIM DE CURSO E NAO CONTROLA EN.
 * - DIR HIGH e considerado SUBIR, como na V10, confirme na montagem.
 * - detach() interrompe PWM, NAO corta a alimentacao do servo.
 * - A massa deve permanecer sem contato com outras partes da estrutura.
 * - Arduino V10 original e EEPROM NAO sao alterados por este sketch.
 */

#include <Arduino.h>
#include <HX711.h>
#include <Servo.h>
#include <math.h>
#include <limits.h>

const byte PINO_DT = A1;
const byte PINO_SCK = A0;
const byte PINO_NTC = A2;
const byte PINO_SERVO = 9;
const byte PINO_DIR = 2;
const byte PINO_STEP = 3;

const float FATOR_CONTAGENS_POR_G = 1058.3167f;
const int ANGULO_REPOUSO = 90;
const int ANGULO_TESTE = 100;
const unsigned int PASSOS_MOTOR = 12;  // Pulsos STEP; nao equivale a mm conhecidos.
const unsigned int PULSO_ALTO_US = 1000;
const unsigned int PULSO_BAIXO_US = 1000;
const unsigned int PAUSA_SERVO_MS = 60;

const unsigned long TEMPO_ESTABILIZACAO_MS = 5000UL;
const unsigned long TIMEOUT_HX_MS = 3000UL;
const byte DESCARTES = 5;
const byte LEITURAS_TARA = 24;
const byte LEITURAS_PONTO = 24;
const byte PONTOS_POR_FASE = 5;
const byte LEITURAS_NTC = 16;

const float NTC_R0 = 10000.0f;
const float NTC_R_FIXO = 10000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_T0_K = 298.15f;
const bool NTC_PARA_GND = true;

HX711 hx;
Servo servo;
long offsetADC = 0;

struct Medicao {
  float adc;
  float peso;
  float dp;
  float amplitude;
  float temperatura;
  bool temperaturaValida;
};

struct Resumo {
  float soma;
  byte quantidade;
  float primeiro;
  float ultimo;
};

// Prototipos explicitos para evitar erros de ordem de structs no Arduino IDE.
void erroFatal(const __FlashStringHelper *mensagem);
void aguardarConfirmacao(char esperado, const __FlashStringHelper *mensagem);
void pausar(unsigned long duracao);
void iniciarHX();
void tararUmaVez();
bool medirTemperatura(float &temperatura);
bool medirPonto(Medicao &medicao);
float media(const Resumo &r);
Resumo executarFase(const char *rotulo);
void imprimirValor(const __FlashStringHelper *rotulo, float valor);
void moverServoLento(int inicio, int fim);
void pulsarMotor(bool subir, unsigned int pulsos);

void erroFatal(const __FlashStringHelper *mensagem) {
  if (servo.attached()) servo.detach();
  digitalWrite(PINO_STEP, LOW);
  Serial.print(F("ERRO:"));
  Serial.println(mensagem);
  Serial.println(F("Se o driver estiver energizado, desligue-o pela chave apropriada."));
  while (true) delay(1000);
}

void aguardarConfirmacao(char esperado, const __FlashStringHelper *mensagem) {
  Serial.println();
  Serial.println(mensagem);
  Serial.print(F("Digite "));
  Serial.print(esperado);
  Serial.println(F(" + ENVIAR (Nova linha). S cancela."));
  while (true) {
    if (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
      if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
      if (c == 'S') erroFatal(F("CANCELADO_PELO_OPERADOR"));
      if (c == esperado) {
        while (Serial.available()) (void)Serial.read();
        Serial.println(F("CONFIRMADO"));
        return;
      }
      Serial.println(F("COMANDO_INVALIDO"));
    }
    delay(10);
  }
}

void pausar(unsigned long duracao) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < duracao) delay(10);
}

void iniciarHX() {
  hx.begin(PINO_DT, PINO_SCK);
  for (byte i = 0; i < DESCARTES; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS))
      erroFatal(F("HX711_TIMEOUT_INICIALIZACAO"));
    (void)hx.read();
  }
  Serial.println(F("HX711:INICIALIZADO"));
}

void tararUmaVez() {
  pausar(3000UL);
  double soma = 0.0;
  for (byte i = 0; i < LEITURAS_TARA; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS))
      erroFatal(F("HX711_TIMEOUT_TARA"));
    soma += hx.read();
  }
  offsetADC = (long)round(soma / LEITURAS_TARA);
  Serial.print(F("TARA_UNICA:OFFSET_ADC:"));
  Serial.println(offsetADC);
}

bool medirTemperatura(float &temperatura) {
  float soma = 0.0f;
  for (byte i = 0; i < LEITURAS_NTC; i++) {
    (void)analogRead(PINO_NTC);  // descarta apos selecao de MUX
    soma += analogRead(PINO_NTC);
    delay(2);
  }
  float adc = soma / (float)LEITURAS_NTC;
  if (adc < 2.0f || adc > 1021.0f) return false;

  float resistencia;
  if (NTC_PARA_GND)
    resistencia = NTC_R_FIXO * adc / (1023.0f - adc);
  else
    resistencia = NTC_R_FIXO * (1023.0f - adc) / adc;

  if (resistencia <= 0.0f) return false;
  float inversoT = (1.0f / NTC_T0_K) + (log(resistencia / NTC_R0) / NTC_BETA);
  temperatura = (1.0f / inversoT) - 273.15f;
  return !isnan(temperatura) && !isinf(temperatura);
}

bool medirPonto(Medicao &m) {
  // Welford: media, desvio-padrao e amplitude de 24 conversoes.
  float mediaADC = 0.0f;
  float M2 = 0.0f;
  long menor = LONG_MAX;
  long maior = LONG_MIN;

  for (byte i = 0; i < LEITURAS_PONTO; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS)) return false;
    long adc = hx.read();
    if (adc < menor) menor = adc;
    if (adc > maior) maior = adc;
    float delta = (float)adc - mediaADC;
    mediaADC += delta / (i + 1);
    M2 += delta * ((float)adc - mediaADC);
  }

  m.adc = mediaADC;
  m.peso = (mediaADC - (float)offsetADC) / FATOR_CONTAGENS_POR_G;
  m.dp = sqrt(M2 / (LEITURAS_PONTO - 1)) / FATOR_CONTAGENS_POR_G;
  m.amplitude = ((float)(maior - menor)) / FATOR_CONTAGENS_POR_G;
  m.temperaturaValida = medirTemperatura(m.temperatura);
  return true;
}

float media(const Resumo &r) {
  return r.quantidade ? r.soma / r.quantidade : NAN;
}

Resumo executarFase(const char *rotulo) {
  Resumo r = {0.0f, 0, 0.0f, 0.0f};
  Serial.println();
  Serial.print(F("FASE:")); Serial.println(rotulo);
  for (byte i = 0; i < PONTOS_POR_FASE; i++) {
    Medicao m;
    if (!medirPonto(m)) erroFatal(F("HX711_TIMEOUT_MEDICAO"));
    if (i == 0) r.primeiro = m.peso;
    r.ultimo = m.peso;
    r.soma += m.peso;
    r.quantidade++;
    Serial.print(F("PONTO:")); Serial.print(rotulo);
    Serial.print(F(":")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(PONTOS_POR_FASE);
    Serial.print(F(":ADC:")); Serial.print(m.adc, 1);
    Serial.print(F(":PESO_G:")); Serial.print(m.peso, 4);
    Serial.print(F(":DP_G:")); Serial.print(m.dp, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(m.amplitude, 4);
    Serial.print(F(":TEMP_C:"));
    if (m.temperaturaValida) Serial.println(m.temperatura, 2);
    else Serial.println(F("INVALIDA"));
  }
  Serial.print(F("MEDIA_FASE:")); Serial.print(rotulo);
  Serial.print(F(":PESO_G:")); Serial.println(media(r), 4);
  return r;
}

void imprimirValor(const __FlashStringHelper *rotulo, float valor) {
  Serial.print(rotulo);
  Serial.print(F(":"));
  Serial.println(valor, 4);
}

void moverServoLento(int inicio, int fim) {
  int direcao = (fim >= inicio) ? 1 : -1;
  for (int angulo = inicio; ; angulo += direcao) {
    servo.write(angulo);
    delay(PAUSA_SERVO_MS);
    if (angulo == fim) break;
  }
}

void pulsarMotor(bool subir, unsigned int pulsos) {
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_DIR, subir ? HIGH : LOW);
  delay(5);
  for (unsigned int i = 0; i < pulsos; i++) {
    digitalWrite(PINO_STEP, HIGH);
    delayMicroseconds(PULSO_ALTO_US);
    digitalWrite(PINO_STEP, LOW);
    delayMicroseconds(PULSO_BAIXO_US);
  }
}

void setup() {
  // STEP LOW antes de tornar o pino saida: evita pulso na inicializacao.
  digitalWrite(PINO_STEP, LOW);
  pinMode(PINO_STEP, OUTPUT);
  digitalWrite(PINO_DIR, LOW);
  pinMode(PINO_DIR, OUTPUT);
  // D6 e D11 NAO sao configurados pelo teste.
  Serial.begin(9600);
  Serial.setTimeout(150);

  Serial.println(F("=========================================="));
  Serial.println(F("TESTE_HX711_SERVO_MOTOR_V10"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | SERVO=D9"));
  Serial.println(F("MOTOR DIR=D2 STEP=D3 | FATOR=1058.3167"));
  Serial.println(F("TARA_UNICA / 5 PONTOS POR FASE / SEM EEPROM"));
  Serial.println(F("A VAZIO | B CARGA | C SERVO PWM | D SERVO MOVE"));
  Serial.println(F("E DETACH | F DRIVER ON | G SERVO+DRIVER ON"));
  Serial.println(F("H DETACH | I MOTOR MOVE | J DRIVER OFF | K ZERO"));
  Serial.println(F("AQUECEDOR FISICAMENTE SEM POTENCIA. D6/D11 NAO CONTROLADOS."));
  Serial.println(F("ATENCAO: SEM FIM DE CURSO; ANGULOS E PULSOS EXIGEM FOLGA."));
  Serial.println(F("=========================================="));

  if (ANGULO_REPOUSO < 0 || ANGULO_REPOUSO > 180 ||
      ANGULO_TESTE < 0 || ANGULO_TESTE > 180)
    erroFatal(F("ANGULOS_FORA_DA_FAIXA"));

  iniciarHX();
  aguardarConfirmacao('C', F("A: PRATO VAZIO. Driver SEM POTENCIA, servo sem PWM. Confirmar tara."));
  tararUmaVez();
  Resumo A = executarFase("A_VAZIO_TUDO_INATIVO");

  aguardarConfirmacao('C', F("B: coloque massa fixa ~36 g, driver DESLIGADO, servo sem PWM."));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo B = executarFase("B_CARGA_TUDO_INATIVO");

  Serial.print(F("ANGULOS_SERVO:")); Serial.print(ANGULO_REPOUSO);
  Serial.print(F("->")); Serial.print(ANGULO_TESTE);
  Serial.print(F("->")); Serial.println(ANGULO_REPOUSO);
  aguardarConfirmacao('P', F("C: CONFIRA o percurso. P ativa o servo em 90 graus. DRIVER OFF."));
  servo.write(ANGULO_REPOUSO);
  servo.attach(PINO_SERVO);
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo C = executarFase("C_SERVO_PWM_DRIVER_OFF");

  aguardarConfirmacao('M', F("D: M autoriza movimento SERVO 90->100->90. DRIVER OFF."));
  moverServoLento(ANGULO_REPOUSO, ANGULO_TESTE);
  moverServoLento(ANGULO_TESTE, ANGULO_REPOUSO);
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo D = executarFase("D_APOS_MOVIMENTO_SERVO");

  servo.detach();
  Serial.println(F("SERVO:DETACH_PWM_DESATIVADO"));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo E = executarFase("E_SERVO_DETACH_DRIVER_OFF");

  aguardarConfirmacao('L', F("F: LIGUE a fonte do DRIVER por chave. Motor IMOVEL. Servo detach. Confirme L."));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo F = executarFase("F_DRIVER_ON_SERVO_DETACH");

  aguardarConfirmacao('P', F("G: CONFIRA a folga. P ativa servo em 90 graus COM DRIVER ENERGIZADO, SEM MOVIMENTO DO MOTOR."));
  servo.write(ANGULO_REPOUSO);
  servo.attach(PINO_SERVO);
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo G = executarFase("G_SERVO_PWM_DRIVER_ON");

  servo.detach();
  Serial.println(F("SERVO:DETACH_PWM_DESATIVADO"));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo H = executarFase("H_SERVO_DETACH_DRIVER_ON");

  Serial.println(F("DIR HIGH=SUBIR (conforme V10). Movimento: 12 pulsos ida + 12 volta."));
  aguardarConfirmacao('M', F("I: M autoriza MOTOR de passo. VERIFIQUE batentes e folga nos dois sentidos."));
  pulsarMotor(true, PASSOS_MOTOR);
  pausar(1000UL);
  pulsarMotor(false, PASSOS_MOTOR);
  digitalWrite(PINO_STEP, LOW);
  Serial.println(F("MOTOR:IDA_E_VOLTA_CONCLUIDA"));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo I = executarFase("I_APOS_MOVIMENTO_MOTOR");

  aguardarConfirmacao('D', F("J: DESLIGUE a fonte do DRIVER pela chave, SEM retirar fios. Confirme D."));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo J = executarFase("J_CARGA_DRIVER_OFF");

  aguardarConfirmacao('C', F("K: retire totalmente a massa. Apenas prato metalico, driver OFF e servo detach."));
  pausar(TEMPO_ESTABILIZACAO_MS);
  Resumo K = executarFase("K_VAZIO_FINAL");

  Serial.println();
  Serial.println(F("========== RELATORIO FINAL =========="));
  imprimirValor(F("A_VAZIO_MEDIA_G"), media(A));
  imprimirValor(F("B_CARGA_INICIAL_MEDIA_G"), media(B));
  imprimirValor(F("C_SERVO_PWM_DRIVER_OFF_MEDIA_G"), media(C));
  imprimirValor(F("D_APOS_SERVO_MEDIA_G"), media(D));
  imprimirValor(F("E_SERVO_DETACH_DRIVER_OFF_MEDIA_G"), media(E));
  imprimirValor(F("F_DRIVER_ON_SERVO_DETACH_MEDIA_G"), media(F));
  imprimirValor(F("G_SERVO_PWM_DRIVER_ON_MEDIA_G"), media(G));
  imprimirValor(F("H_SERVO_DETACH_DRIVER_ON_MEDIA_G"), media(H));
  imprimirValor(F("I_APOS_MOTOR_MEDIA_G"), media(I));
  imprimirValor(F("J_DRIVER_OFF_MEDIA_G"), media(J));
  imprimirValor(F("K_VAZIO_FINAL_MEDIA_G"), media(K));
  Serial.println(F("--------------------------------------"));
  imprimirValor(F("DELTA_SERVO_PWM_COM_DRIVER_OFF_G"), media(C) - media(B));
  imprimirValor(F("DELTA_APOS_MOVIMENTO_SERVO_G"), media(D) - media(C));
  imprimirValor(F("DELTA_SERVO_DETACH_DRIVER_OFF_G"), media(E) - media(D));
  imprimirValor(F("DELTA_DRIVER_ON_SEM_SERVO_PWM_G"), media(F) - media(E));
  imprimirValor(F("DELTA_SERVO_PWM_COM_DRIVER_ON_G"), media(G) - media(F));
  imprimirValor(F("DELTA_DETACH_COM_DRIVER_ON_G"), media(H) - media(G));
  imprimirValor(F("DELTA_APOS_MOTOR_G"), media(I) - media(H));
  imprimirValor(F("DELTA_DRIVER_OFF_FINAL_G"), media(J) - media(I));
  imprimirValor(F("DELTA_CARGA_FINAL_MENOS_INICIAL_G"), media(J) - media(B));
  imprimirValor(F("RESIDUO_ZERO_FINAL_MENOS_INICIAL_G"), media(K) - media(A));
  Serial.println(F("NOTA: MOVIMENTOS MEDIDOS APOS ACOMODACAO, NAO DURANTE."));
  Serial.println(F("NOTA: DIFERENCAS PODEM INCLUIR DERIVA TEMPORAL E MECANICA."));
  Serial.println(F("NOTA: DETACH NAO DESENERGIZA O SERVO; EN DO DRIVER NAO E CONTROLADO."));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void loop() {
  delay(1000);
}
