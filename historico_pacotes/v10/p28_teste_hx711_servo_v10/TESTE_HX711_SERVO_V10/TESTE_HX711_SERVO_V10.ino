/*
 * TESTE_HX711_SERVO_V10.ino
 * Arduino_Grao_CNN - Diagnostico isolado do servo e pesagem
 *
 * Arduino Uno: HX711 DT=A1, SCK=A0; servo D9; NTC A2.
 * Biblioteca: HX711 (bogde) e Servo (Arduino).
 * Serial: 9600 baud / Nova linha.
 *
 * FASES:
 * A - prato vazio, servo sem PWM (detach), tara unica
 * B - carga fixa ~36 g, servo sem PWM
 * C - mesma carga, servo ativado e parado em ANGULO_REPOUSO
 * D - mesma carga, servo move e retorna; PWM continua ativo
 * E - mesma carga, servo desativado (detach), apos 5 s
 * F - prato vazio de novo, servo sem PWM
 *
 * SEGURANCA:
 *  - Aquecedor fisicamente SEM alimentacao de potencia.
 *  - Motor de passo SEM alimentacao de potencia.
 *  - A plataforma deve permanecer fixa; nao tocar no prato ao medir.
 *  - O teste NAO configura D6 nem D11 (podem acionar reles ativos em LOW).
 *  - Servo alimentado por 5 V dimensionados para sua corrente;
 *    GND da fonte do servo e do Arduino em comum.
 *  - CONFIRA OS ANGULOS SEGUROS antes de ativar o servo!
 *    O movimento e intencional e pode deslocar a mecanica.
 *  - detach() DESLIGA O PWM, NAO a alimentacao eletrica do servo.
 *
 * Nao mexe na EEPROM nem no firmware principal da automacao V10.
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

// Fator PROVISORIO calculado a partir da referencia domestica ~267 g.
const float FATOR_CONTAGENS_POR_G = 1058.3167f;

// ANGULOS DE TESTE: ajuste SOMENTE apos conferir folga mecanica.
// No firmware V10, 90 graus era usado como posicao intermediaria.
const int ANGULO_REPOUSO = 90;
const int ANGULO_TESTE = 100;  // deslocamento de 10 graus
const unsigned int PAUSA_PASSO_SERVO_MS = 60;

const byte DESCARTES = 5;
const byte LEITURAS_TARA = 24;
const byte LEITURAS_PONTO = 24;
const byte PONTOS_POR_FASE = 5;
const unsigned long TIMEOUT_HX_MS = 3000UL;
const unsigned long TEMPO_ACOMODACAO_MS = 5000UL;

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
  float ruidoDP;
  float amplitude;
  float tempC;
  bool tempOK;
};

struct Resumo {
  float somaPesos;
  byte pontos;
  float primeiroPeso;
  float ultimoPeso;
};

// Prototipos explicitos para compatibilidade com o pre-processador Arduino.
void pararComErro(const __FlashStringHelper *mensagem);
void aguardarComando(char esperado, const __FlashStringHelper *instrucao);
void esperar(unsigned long ms);
void inicializarHX();
void fazerTara();
bool lerTemperatura(float &tempC);
bool fazerMedicao(Medicao &saida);
Resumo executarFase(const char *nome);
float mediaFase(const Resumo &resumo);
void imprimirDiferenca(const __FlashStringHelper *nome, float valor);
void moverServoDevagar(int inicial, int final);

void pararComErro(const __FlashStringHelper *mensagem) {
  if (servo.attached()) servo.detach();
  Serial.print(F("ERRO:"));
  Serial.println(mensagem);
  while (true) delay(1000);
}

void aguardarComando(char esperado, const __FlashStringHelper *instrucao) {
  Serial.println();
  Serial.println(instrucao);
  Serial.print(F("Digite "));
  Serial.print(esperado);
  Serial.println(F(" + ENVIAR (Nova linha). Digite S para cancelar."));
  while (true) {
    if (Serial.available()) {
      char c = (char) Serial.read();
      if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
      if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
      if (c == 'S') pararComErro(F("TESTE_CANCELADO_PELO_USUARIO"));
      if (c == esperado) {
        // Consome terminadores que ja chegaram, sem exigir outro comando.
        while (Serial.available()) Serial.read();
        Serial.println(F("CONFIRMADO"));
        return;
      }
      Serial.println(F("COMANDO_INVALIDO"));
    }
    delay(10);
  }
}

void esperar(unsigned long ms) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < ms) delay(10);
}

void inicializarHX() {
  hx.begin(PINO_DT, PINO_SCK);
  for (byte i = 0; i < DESCARTES; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS))
      pararComErro(F("HX711_SEM_RESPOSTA_DURANTE_INICIALIZACAO"));
    (void)hx.read();
  }
  Serial.println(F("HX711:INICIALIZADO"));
}

void fazerTara() {
  // Uma UNICA tara em vazio. Nunca recalibra durante as fases.
  esperar(3000UL);
  double soma = 0.0;
  for (byte i = 0; i < LEITURAS_TARA; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS))
      pararComErro(F("HX711_TIMEOUT_TARA"));
    soma += hx.read();
  }
  offsetADC = (long)round(soma / LEITURAS_TARA);
  Serial.print(F("TARA_UNICA:OFFSET_ADC:"));
  Serial.println(offsetADC);
}

bool lerTemperatura(float &tempC) {
  float soma = 0.0f;
  for (byte i = 0; i < 16; i++) {
    (void)analogRead(PINO_NTC);  // descarta primeira conversao apos trocar o MUX
    soma += analogRead(PINO_NTC);
    delay(2);
  }
  float adc = soma / 16.0f;
  if (adc < 2.0f || adc > 1021.0f) return false;
  float rNTC;
  if (NTC_PARA_GND) {
    rNTC = NTC_R_FIXO * adc / (1023.0f - adc);
  } else {
    rNTC = NTC_R_FIXO * (1023.0f - adc) / adc;
  }
  if (rNTC <= 0.0f) return false;
  float invT = (1.0f / NTC_T0_K) + (log(rNTC / NTC_R0) / NTC_BETA);
  tempC = (1.0f / invT) - 273.15f;
  return !isnan(tempC) && !isinf(tempC);
}

bool fazerMedicao(Medicao &saida) {
  // Welford: desvio-padrao da leitura individual, sem overflow de ADC^2.
  float mediaADC = 0.0f;
  float M2 = 0.0f;
  long minimo = LONG_MAX;
  long maximo = LONG_MIN;
  for (byte i = 0; i < LEITURAS_PONTO; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS)) return false;
    long adc = hx.read();
    if (adc < minimo) minimo = adc;
    if (adc > maximo) maximo = adc;
    float delta = ((float)adc) - mediaADC;
    mediaADC += delta / (i + 1);
    M2 += delta * (((float)adc) - mediaADC);
  }
  saida.adc = mediaADC;
  saida.peso = (mediaADC - (float)offsetADC) / FATOR_CONTAGENS_POR_G;
  saida.ruidoDP = sqrt(M2 / (LEITURAS_PONTO - 1)) / FATOR_CONTAGENS_POR_G;
  saida.amplitude = ((float)(maximo - minimo)) / FATOR_CONTAGENS_POR_G;
  saida.tempOK = lerTemperatura(saida.tempC);
  return true;
}

Resumo executarFase(const char *nome) {
  Resumo r = {0.0f, 0, 0.0f, 0.0f};
  Serial.println();
  Serial.print(F("FASE:"));
  Serial.println(nome);
  for (byte i = 0; i < PONTOS_POR_FASE; i++) {
    Medicao med;
    if (!fazerMedicao(med)) pararComErro(F("HX711_TIMEOUT_MEDICAO"));
    if (i == 0) r.primeiroPeso = med.peso;
    r.ultimoPeso = med.peso;
    r.somaPesos += med.peso;
    r.pontos++;
    Serial.print(F("PONTO:")); Serial.print(nome);
    Serial.print(F(":")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(PONTOS_POR_FASE);
    Serial.print(F(":ADC:")); Serial.print(med.adc, 1);
    Serial.print(F(":PESO_G:")); Serial.print(med.peso, 4);
    Serial.print(F(":DP_G:")); Serial.print(med.ruidoDP, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(med.amplitude, 4);
    Serial.print(F(":TEMP_C:"));
    if (med.tempOK) Serial.println(med.tempC, 2);
    else Serial.println(F("INVALIDA"));
  }
  Serial.print(F("MEDIA_FASE:")); Serial.print(nome);
  Serial.print(F(":PESO_G:")); Serial.println(mediaFase(r), 4);
  return r;
}

float mediaFase(const Resumo &resumo) {
  return resumo.pontos ? resumo.somaPesos / resumo.pontos : NAN;
}

void imprimirDiferenca(const __FlashStringHelper *nome, float valor) {
  Serial.print(nome);
  Serial.print(F(":"));
  Serial.println(valor, 4);
}

void moverServoDevagar(int inicial, int final) {
  int passo = (final >= inicial) ? 1 : -1;
  for (int angulo = inicial; ; angulo += passo) {
    servo.write(angulo);
    delay(PAUSA_PASSO_SERVO_MS);
    if (angulo == final) break;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(150);
  // Servo permanece sem PWM. Nao toca D6/D11 nem nos pinos do motor.
  Serial.println(F("=========================================="));
  Serial.println(F("TESTE_HX711_SERVO_V10"));
  Serial.println(F("HX711 DT=A1 SCK=A0; NTC=A2; SERVO=D9"));
  Serial.println(F("FATOR_PROVISORIO:1058.3167_CONTAGENS/G"));
  Serial.println(F("A VAZIO | B CARGA | C SERVO PARADO ATIVO"));
  Serial.println(F("D SERVO MOVE E VOLTA | E DETACH | F VAZIO"));
  Serial.println(F("SEM AQUECEDOR / SEM MOTOR DE PASSO / TARA UNICA"));
  Serial.println(F("AVISO: DETACH NAO CORTA A ENERGIA DO SERVO"));
  Serial.println(F("=========================================="));

  if (ANGULO_REPOUSO < 0 || ANGULO_REPOUSO > 180 ||
      ANGULO_TESTE < 0 || ANGULO_TESTE > 180)
    pararComErro(F("ANGULOS_FORA_DA_FAIXA"));
  inicializarHX();
  aguardarComando('C', F("A: PRATO VAZIO. Servo sem PWM. Confirmar para tarar."));
  fazerTara();
  Resumo A = executarFase("A_VAZIO_SEM_PWM");

  aguardarComando('C', F("B: coloque carga fixa ~36 g e nao toque no prato."));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo B = executarFase("B_CARGA_SEM_PWM");

  Serial.print(F("ANGULO_REPOUSO:")); Serial.println(ANGULO_REPOUSO);
  Serial.print(F("ANGULO_TESTE:")); Serial.println(ANGULO_TESTE);
  aguardarComando('P', F("C: CONFIRA folga mecanica. P ativa servo em 90 graus."));
  servo.write(ANGULO_REPOUSO);
  servo.attach(PINO_SERVO);
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo C = executarFase("C_CARGA_PWM_PARADO");

  aguardarComando('M', F("D: CONFIRA SEGURANCA. M move servo ida/volta 10 graus."));
  moverServoDevagar(ANGULO_REPOUSO, ANGULO_TESTE);
  moverServoDevagar(ANGULO_TESTE, ANGULO_REPOUSO);
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo D = executarFase("D_CARGA_APOS_MOVIMENTO_PWM");

  servo.detach();
  Serial.println(F("SERVO:DETACH_PWM_DESATIVADO"));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo E = executarFase("E_CARGA_APOS_DETACH");

  aguardarComando('C', F("F: retire a massa. Deixe o prato instalado."));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo F = executarFase("F_VAZIO_APOS_CARGA");

  Serial.println();
  Serial.println(F("========== RELATORIO FINAL =========="));
  imprimirDiferenca(F("A_VAZIO_MEDIA_G"), mediaFase(A));
  imprimirDiferenca(F("B_CARGA_SEM_PWM_MEDIA_G"), mediaFase(B));
  imprimirDiferenca(F("C_CARGA_PWM_PARADO_MEDIA_G"), mediaFase(C));
  imprimirDiferenca(F("D_CARGA_APOS_MOVIMENTO_MEDIA_G"), mediaFase(D));
  imprimirDiferenca(F("E_CARGA_APOS_DETACH_MEDIA_G"), mediaFase(E));
  imprimirDiferenca(F("F_VAZIO_FINAL_MEDIA_G"), mediaFase(F));
  imprimirDiferenca(F("DELTA_PWM_PARADO_MENOS_SEM_PWM_G"), mediaFase(C) - mediaFase(B));
  imprimirDiferenca(F("DELTA_MOVIMENTO_MENOS_PWM_PARADO_G"), mediaFase(D) - mediaFase(C));
  imprimirDiferenca(F("DELTA_DETACH_MENOS_POS_MOVIMENTO_G"), mediaFase(E) - mediaFase(D));
  imprimirDiferenca(F("RESIDUO_ZERO_FINAL_MENOS_INICIAL_G"), mediaFase(F) - mediaFase(A));
  Serial.println(F("NOTA: variacoes entre fases tambem podem incluir deriva temporal."));
  Serial.println(F("NOTA: ensaio nao demonstra causalidade isolada sem repeticoes."));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void loop() {
  delay(1000);
}
