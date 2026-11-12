/*
 * TESTE_HX711_MOTOR_PASSO_V10.ino
 * Arduino_Grao_CNN - Diagnostico isolado HX711 + motor de passo.
 *
 * Arduino Uno:
 *   HX711 DT=A1, SCK=A0 (biblioteca HX711, bogde)
 *   Motor: DIR=D2, STEP=D3 (driver externo, conforme V10)
 *   NTC=A2 (opcional, em contato proximo a celula)
 *   Serial=9600 baud, monitor em "Nova linha".
 *
 * Etapas:
 * A: prato vazio, alimentacao de potencia do driver DESLIGADA, tara unica.
 * B: carga fixa (~36 g), driver DESLIGADO.
 * C: mesma carga, driver ALIMENTADO/ATIVO, sem comandos de passo.
 * D: 12 pulsos no sentido DIR=HIGH, pausa, 12 pulsos de volta DIR=LOW;
 *    medir APOS a volta e a acomodacao.
 * E: mesma carga, driver DESLIGADO novamente.
 * F: prato vazio, driver DESLIGADO, medir retorno ao zero.
 *
 * SEGURANCA:
 * - NAO ENERGIZAR A RESISTENCIA DE AQUECIMENTO; fonte de potencia do
 *   aquecedor fisicamente desligada. O codigo NAO controla D6/D11.
 * - Servo D9 SEM alimentacao (nao e controlado por este diagnostico).
 * - Desligar e ligar a FONTE DO DRIVER somente por chave apropriada.
 *   NUNCA conectar/desconectar fios do motor/driver com alimentacao ligada.
 * - Arduino/driver devem compartilhar GND de referencia, conforme montagem.
 * - O codigo NAO altera EN do driver. Para fase C e D, driver deve estar
 *   configurado para habilitar o motor conforme sua montagem original.
 * - O sentido DIR=HIGH era "subir" na V10; verificar folga para esse sentido!
 * - Nao existe leitura de fim de curso neste diagnostico.
 *   Verificar FISICAMENTE a distancia ate os batentes em ambos os sentidos.
 * - Os movimentos so ocorrem apos confirmacao M no Monitor Serial.
 * - Nenhuma escrita na EEPROM. Firmware principal V10 NAO alterado.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <limits.h>

const byte PINO_HX_DT = A1;
const byte PINO_HX_SCK = A0;
const byte PINO_NTC = A2;
const byte PINO_DIR = 2;
const byte PINO_STEP = 3;

const float FATOR_CONTAGENS_POR_G = 1058.3167f; // Provisorio

// 12 passos do sinal STEP (microsteps, se o driver usar microstepping).
// A distancia REAL depende do driver, do motor e do fuso.
const unsigned int PASSOS_MOVIMENTO = 12;
const unsigned int PULSO_ALTO_US = 1000;
const unsigned int PULSO_BAIXO_US = 1000;
const unsigned long TEMPO_ACOMODACAO_MS = 5000UL;
const unsigned long TIMEOUT_HX_MS = 3000UL;
const byte LEITURAS_TARA = 24;
const byte LEITURAS_PONTO = 24;
const byte PONTOS_POR_FASE = 5;

const float NTC_R0 = 10000.0f;
const float NTC_R_FIXO = 10000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_T0_K = 298.15f;
const bool NTC_PARA_GND = true;

HX711 hx;
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

// Prototipos explicitos: evita erros do pre-processador Arduino com structs.
void pararComErro(const __FlashStringHelper *mensagem);
void aguardarComando(char esperado, const __FlashStringHelper *instrucao);
void esperar(unsigned long ms);
void inicializarHX();
void fazerTara();
bool lerTemperatura(float &tempC);
bool fazerMedicao(Medicao &saida);
Resumo executarFase(const char *nome);
float mediaFase(const Resumo &r);
void imprimirResultado(const __FlashStringHelper *nome, float valor);
void emitirPassos(bool subir, unsigned int quantidade);

void pararComErro(const __FlashStringHelper *mensagem) {
  digitalWrite(PINO_STEP, LOW);
  Serial.print(F("ERRO:"));
  Serial.println(mensagem);
  Serial.println(F("SE DRIVER ESTIVER ALIMENTADO, DESLIGUE SUA FONTE POR CHAVE."));
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
      char c = (char)Serial.read();
      if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
      if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
      if (c == 'S') pararComErro(F("TESTE_CANCELADO_PELO_OPERADOR"));
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

void esperar(unsigned long ms) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < ms) delay(10);
}

void inicializarHX() {
  hx.begin(PINO_HX_DT, PINO_HX_SCK);
  for (byte i = 0; i < 5; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS))
      pararComErro(F("HX711_TIMEOUT_INICIALIZACAO"));
    (void)hx.read();
  }
  Serial.println(F("HX711:INICIALIZADO"));
}

void fazerTara() {
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
    (void)analogRead(PINO_NTC);
    soma += analogRead(PINO_NTC);
    delay(2);
  }
  float adc = soma / 16.0f;
  if (adc < 2.0f || adc > 1021.0f) return false;
  float rNTC;
  if (NTC_PARA_GND)
    rNTC = NTC_R_FIXO * adc / (1023.0f - adc);
  else
    rNTC = NTC_R_FIXO * (1023.0f - adc) / adc;
  if (rNTC <= 0.0f) return false;
  float invT = (1.0f / NTC_T0_K) + log(rNTC / NTC_R0) / NTC_BETA;
  tempC = 1.0f / invT - 273.15f;
  return !isnan(tempC) && !isinf(tempC);
}

bool fazerMedicao(Medicao &saida) {
  // Media + DP amostral pelo algoritmo de Welford.
  float mediaADC = 0.0f;
  float M2 = 0.0f;
  long minimo = LONG_MAX;
  long maximo = LONG_MIN;
  for (byte i = 0; i < LEITURAS_PONTO; i++) {
    if (!hx.wait_ready_timeout(TIMEOUT_HX_MS)) return false;
    long adc = hx.read();
    if (adc < minimo) minimo = adc;
    if (adc > maximo) maximo = adc;
    float delta = (float)adc - mediaADC;
    mediaADC += delta / (i + 1);
    M2 += delta * ((float)adc - mediaADC);
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
  Serial.print(F("FASE:")); Serial.println(nome);
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

float mediaFase(const Resumo &r) {
  return r.pontos ? r.somaPesos / r.pontos : NAN;
}

void imprimirResultado(const __FlashStringHelper *nome, float valor) {
  Serial.print(nome); Serial.print(F(":")); Serial.println(valor, 4);
}

void emitirPassos(bool subir, unsigned int quantidade) {
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_DIR, subir ? HIGH : LOW);
  delay(5); // Tempo para estabilizacao da direcao do driver.
  for (unsigned int i = 0; i < quantidade; i++) {
    digitalWrite(PINO_STEP, HIGH);
    delayMicroseconds(PULSO_ALTO_US);
    digitalWrite(PINO_STEP, LOW);
    delayMicroseconds(PULSO_BAIXO_US);
  }
}

void setup() {
  // Inicializar STEP no nivel baixo: nenhum movimento por programa ao ligar.
  digitalWrite(PINO_STEP, LOW);
  pinMode(PINO_STEP, OUTPUT);
  digitalWrite(PINO_DIR, LOW);
  pinMode(PINO_DIR, OUTPUT);
  Serial.begin(9600);
  Serial.setTimeout(150);

  Serial.println(F("=========================================="));
  Serial.println(F("TESTE_HX711_MOTOR_PASSO_V10"));
  Serial.println(F("HX711 DT=A1 SCK=A0; NTC=A2; DIR=D2 STEP=D3"));
  Serial.println(F("FATOR_PROVISORIO:1058.3167_CONTAGENS/G"));
  Serial.println(F("A VAZIO DRIVER OFF | B CARGA DRIVER OFF"));
  Serial.println(F("C CARGA DRIVER ON SEM PULSOS"));
  Serial.println(F("D CARGA APOS 12 PASSOS IDA + 12 VOLTA"));
  Serial.println(F("E CARGA DRIVER OFF | F ZERO FINAL"));
  Serial.println(F("SEM AQUECEDOR / SEM SERVO / TARA UNICA / SEM EEPROM"));
  Serial.println(F("CUIDADO: D6/D11 NAO CONTROLADOS; RESISTENCIA SEM POTENCIA!"));
  Serial.println(F("MOTOR: NAO POSSUI FIM DE CURSO MONITORADO NESTE TESTE."));
  Serial.println(F("=========================================="));

  inicializarHX();
  aguardarComando('C', F("A: PRATO VAZIO e FONTE DRIVER DESLIGADA. CONFIRME."));
  fazerTara();
  Resumo A = executarFase("A_VAZIO_DRIVER_OFF");

  aguardarComando('C', F("B: coloque massa ~36 g. DRIVER ainda DESLIGADO."));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo B = executarFase("B_CARGA_DRIVER_OFF");

  aguardarComando('L', F("C: ligue a FONTE do driver pela CHAVE. NAO toque nos cabos.\nConfirme L so quando motor estiver habilitado e IMOVEL."));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo C = executarFase("C_CARGA_DRIVER_ON_PARADO");

  Serial.println(F("DIR HIGH=SUBIR conforme V10. 12 pulsos ida + 12 volta."));
  aguardarComando('M', F("D: CONFIRA FOLGA/SEM BATENTES e percurso livre. M autoriza movimento."));
  emitirPassos(true, PASSOS_MOVIMENTO);
  esperar(1000UL);
  emitirPassos(false, PASSOS_MOVIMENTO);
  digitalWrite(PINO_STEP, LOW);
  Serial.println(F("MOTOR:IDA_E_VOLTA_CONCLUIDA"));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo D = executarFase("D_CARGA_APOS_IDA_VOLTA");

  aguardarComando('D', F("E: DESLIGUE a FONTE do driver pela CHAVE, SEM DESCONECTAR FIOS."));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo E = executarFase("E_CARGA_DRIVER_OFF");

  aguardarComando('C', F("F: retire a massa; so prato metalico, DRIVER DESLIGADO."));
  esperar(TEMPO_ACOMODACAO_MS);
  Resumo F = executarFase("F_VAZIO_FINAL");

  Serial.println();
  Serial.println(F("========== RELATORIO FINAL =========="));
  imprimirResultado(F("A_VAZIO_OFF_MEDIA_G"), mediaFase(A));
  imprimirResultado(F("B_CARGA_OFF_MEDIA_G"), mediaFase(B));
  imprimirResultado(F("C_CARGA_DRIVER_ON_MEDIA_G"), mediaFase(C));
  imprimirResultado(F("D_CARGA_APOS_MOVIMENTO_MEDIA_G"), mediaFase(D));
  imprimirResultado(F("E_CARGA_DRIVER_OFF_MEDIA_G"), mediaFase(E));
  imprimirResultado(F("F_VAZIO_FINAL_MEDIA_G"), mediaFase(F));
  imprimirResultado(F("DELTA_DRIVER_ON_MENOS_OFF_G"), mediaFase(C) - mediaFase(B));
  imprimirResultado(F("DELTA_MOVIMENTO_MENOS_ON_PARADO_G"), mediaFase(D) - mediaFase(C));
  imprimirResultado(F("DELTA_DRIVER_OFF_MENOS_APOS_MOVIMENTO_G"), mediaFase(E) - mediaFase(D));
  imprimirResultado(F("RESIDUO_ZERO_FINAL_MENOS_INICIAL_G"), mediaFase(F) - mediaFase(A));
  Serial.println(F("NOTA: FASE D MEDE APOS MOVIMENTO; NAO DURANTE O MOVIMENTO."));
  Serial.println(F("NOTA: FUSO/FOLGAS E DERIVA TEMPORAL PODEM ALTERAR AS MEDIAS."));
  Serial.println(F("NOTA: DRIVER EN NAO E CONTROLADO POR ESTE CODIGO."));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void loop() {
  delay(1000);
}
