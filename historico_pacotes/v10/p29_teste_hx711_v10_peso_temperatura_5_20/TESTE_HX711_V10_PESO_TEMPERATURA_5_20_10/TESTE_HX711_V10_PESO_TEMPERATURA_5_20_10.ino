/*
 * Arduino_Grao_CNN - HX711 V10 - Diagnostico de peso + temperatura NTC
 *
 * A (vazio): 5 minutos, 11 pontos.
 * B (carga fixa ~33,4 g): 20 minutos, 41 pontos.
 * C (vazio apos carga): 10 minutos, 21 pontos.
 * Cada ponto: media de 24 conversoes HX711 + leitura do NTC A2.
 * Uma tara inicial, SEM compensar deriva e SEM mexer no firmware do TCC.
 *
 * PINOS: HX711 DT=A1, SCK=A0; NTC=A2; D2=DIR, D3=STEP, D9=servo.
 * Divisor esperado (firmware original do TCC): resistor fixo 10k entre
 * 5V e A2; NTC 10k entre A2 e GND; beta 3950, R0=10k a 25C.
 * Se a montagem for invertida, altere NTC_PARA_GND para false.
 *
 * ATENCAO: D6 e D11 LOW podem ACIONAR reles ativos em LOW.
 * O AQUECEDOR PRECISA ESTAR FISICAMENTE SEM ALIMENTACAO DE POTENCIA.
 * Nao aciona motor, servo ou resistencia. Plataforma imovel em X.
 * NTC mede temperatura LOCAL ONDE ESTA INSTALADO, nao necessariamente
 * a temperatura do ambiente. Nao medir o interior aquecido como ambiente.
 *
 * Arduino Uno; biblioteca HX711 (API begin, read, wait_ready_timeout).
 * Monitor Serial 9600 baud, final de linha: Nova linha.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

const byte PINO_DT = A1;
const byte PINO_SCK = A0;
const byte PINO_NTC = A2;
const byte PINO_DIR = 2;
const byte PINO_STEP = 3;
const byte PINO_SERVO = 9;
const byte PINO_RELE = 6;
const byte PINO_AQUECEDOR = 11;

const float FATOR_CONTAGENS_POR_G = -1129.6755f; // provisoria
const float NTC_R0_OHMS = 10000.0f;
const float NTC_R_FIXO_OHMS = 10000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_T0_K = 298.15f;
const bool NTC_PARA_GND = true; // true: NTC para GND, resistor fixo para +5V

const unsigned long INTERVALO_MS = 30000UL;
const unsigned long TIMEOUT_HX_MS = 2500UL;
const byte DESCARTES = 5;
const byte LEITURAS_TARA = 20;
const byte LEITURAS_PONTO = 24;
const byte AMOSTRAS_NTC = 16;
const byte PONTOS_A = 11;
const byte PONTOS_B = 41;
const byte PONTOS_C = 21;

HX711 balanca;
long offsetADC = 0;
float pesosA[PONTOS_A], pesosB[PONTOS_B], pesosC[PONTOS_C];
float tempA[PONTOS_A], tempB[PONTOS_B], tempC[PONTOS_C];

struct Medicao {
  float peso;
  float dp;
  float amplitude;
  float adcMedio;
  float tempC;
  float ntcADC;
  bool ntcValido;
};

// Prototipos declarados APOS Medicao: compatibilidade com pre-processador IDE.
void manterAtuadoresParados();
void configurarPinos();
char aguardarComando();
bool confirmar(const __FlashStringHelper *mensagem);
bool lerADC(long &adc);
bool lerNTC(float &temperaturaC, float &adcMedio);
bool medir(Medicao &m);
void imprimir(const __FlashStringHelper *nome, float valor);
void exibirPonto(const __FlashStringHelper *fase, byte indice, byte total, const Medicao &m);
bool coletar(const __FlashStringHelper *fase, byte total, float *pesos, float *temps);
float mediaIntervalo(const float *v, byte inicio, byte fim);
void resumoFase(const __FlashStringHelper *fase, const float *pesos, const float *temps, byte n);
void relatorio();
void setup();
void loop();

void manterAtuadoresParados() {
  digitalWrite(PINO_DIR, LOW);
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_SERVO, LOW);
  digitalWrite(PINO_RELE, LOW);      // LOW pode LIGAR rele ativo-baixo!
  digitalWrite(PINO_AQUECEDOR, LOW); // Alimentacao de potencia isolada!
}

void configurarPinos() {
  manterAtuadoresParados();
  pinMode(PINO_DIR, OUTPUT);
  pinMode(PINO_STEP, OUTPUT);
  pinMode(PINO_SERVO, OUTPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_AQUECEDOR, OUTPUT);
  pinMode(PINO_NTC, INPUT);
  manterAtuadoresParados();
}

char aguardarComando() {
  char linha[12];
  byte n = 0;
  while (true) {
    manterAtuadoresParados();
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        linha[n] = '\0';
        if (n == 1 && (linha[0] == 'C' || linha[0] == 'c')) return 'C';
        if (strcmp(linha, "SAIR") == 0 || strcmp(linha, "sair") == 0) return 'S';
        if (n > 0) Serial.println(F("COMANDO_INVALIDO: C ou SAIR"));
        n = 0;
      } else if (n < sizeof(linha) - 1) {
        linha[n++] = c;
      } else {
        n = 0;
        Serial.println(F("COMANDO_MUITO_LONGO"));
      }
    }
    delay(20);
  }
}

bool confirmar(const __FlashStringHelper *mensagem) {
  while (Serial.available()) Serial.read();
  Serial.println();
  Serial.println(mensagem);
  Serial.println(F("Digite C + ENVIAR (Nova linha); SAIR cancela."));
  if (aguardarComando() != 'C') {
    Serial.println(F("TESTE:CANCELADO"));
    return false;
  }
  Serial.println(F("CONFIRMADO"));
  return true;
}

bool lerADC(long &adc) {
  if (!balanca.wait_ready_timeout(TIMEOUT_HX_MS)) {
    Serial.println(F("ERRO:HX711_NAO_RESPONDEU"));
    return false;
  }
  adc = balanca.read();
  return true;
}

// NTC ao GND e resistor fixo ao 5V:
// R_NTC = R_FIXO * ADC / (1023 - ADC).
// Relacao em ADC cancela dependencia direta do Vcc nominal (ratiometrico).
bool lerNTC(float &temperaturaC, float &adcMedio) {
  long soma = 0;
  analogRead(PINO_NTC); // primeira conversao de acomodacao do multiplexador
  for (byte i = 0; i < AMOSTRAS_NTC; ++i) {
    soma += analogRead(PINO_NTC);
    delay(2);
  }
  adcMedio = (float)soma / (float)AMOSTRAS_NTC;
  if (adcMedio <= 1.0f || adcMedio >= 1022.0f) {
    temperaturaC = NAN;
    return false;
  }
  float resistencia;
  if (NTC_PARA_GND) {
    resistencia = NTC_R_FIXO_OHMS * adcMedio / (1023.0f - adcMedio);
  } else {
    resistencia = NTC_R_FIXO_OHMS * (1023.0f - adcMedio) / adcMedio;
  }
  if (resistencia <= 0.0f) {
    temperaturaC = NAN;
    return false;
  }
  float inversoK = 1.0f / NTC_T0_K + logf(resistencia / NTC_R0_OHMS) / NTC_BETA;
  if (inversoK <= 0.0f) {
    temperaturaC = NAN;
    return false;
  }
  temperaturaC = 1.0f / inversoK - 273.15f;
  return !isnan(temperaturaC) && !isinf(temperaturaC);
}

bool medir(Medicao &m) {
  float mediaG = 0.0f, mediaADC = 0.0f, m2 = 0.0f;
  float minimo = 0.0f, maximo = 0.0f;
  for (byte k = 0; k < LEITURAS_PONTO; ++k) {
    long adc;
    if (!lerADC(adc)) return false;
    float peso = (float)((int64_t)adc - (int64_t)offsetADC) / FATOR_CONTAGENS_POR_G;
    if (k == 0 || peso < minimo) minimo = peso;
    if (k == 0 || peso > maximo) maximo = peso;
    float delta = peso - mediaG;
    mediaG += delta / (float)(k + 1);
    m2 += delta * (peso - mediaG);
    mediaADC += ((float)adc - mediaADC) / (float)(k + 1);
  }
  m.peso = mediaG;
  m.dp = sqrtf(m2 / (float)(LEITURAS_PONTO - 1));
  m.amplitude = maximo - minimo;
  m.adcMedio = mediaADC;
  m.ntcValido = lerNTC(m.tempC, m.ntcADC);
  return true;
}

void imprimir(const __FlashStringHelper *nome, float valor) {
  Serial.print(nome);
  Serial.println(valor, 4);
}

void exibirPonto(const __FlashStringHelper *fase, byte indice, byte total, const Medicao &m) {
  Serial.print(F("PONTO:")); Serial.print(fase);
  Serial.print(F(":")); Serial.print(indice + 1);
  Serial.print(F("/")); Serial.print(total);
  Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)indice * (INTERVALO_MS / 1000UL));
  Serial.print(F(":PESO_MEDIO_G:")); Serial.print(m.peso, 4);
  Serial.print(F(":RUIDO_DP_G:")); Serial.print(m.dp, 4);
  Serial.print(F(":AMPLITUDE_G:")); Serial.print(m.amplitude, 4);
  Serial.print(F(":ADC_MEDIO:")); Serial.print(m.adcMedio, 1);
  Serial.print(F(":NTC_ADC:")); Serial.print(m.ntcADC, 1);
  Serial.print(F(":TEMP_NTC_C:"));
  if (m.ntcValido) Serial.print(m.tempC, 2);
  else Serial.print(F("INVALIDA"));
  Serial.print(F(":TEMP_STATUS:"));
  Serial.println(m.ntcValido ? F("OK") : F("ERRO"));
}

bool coletar(const __FlashStringHelper *fase, byte total, float *pesos, float *temps) {
  unsigned long inicio = millis();
  for (byte i = 0; i < total; ++i) {
    unsigned long alvo = (unsigned long)i * INTERVALO_MS;
    while ((unsigned long)(millis() - inicio) < alvo) {
      manterAtuadoresParados();
      delay(20);
    }
    Medicao m;
    if (!medir(m)) return false;
    pesos[i] = m.peso;
    temps[i] = m.ntcValido ? m.tempC : NAN;
    exibirPonto(fase, i, total, m);
  }
  return true;
}

float mediaIntervalo(const float *v, byte inicio, byte fim) {
  float soma = 0.0f;
  byte validos = 0;
  for (byte i = inicio; i <= fim; ++i) {
    if (isnan(v[i])) continue;
    soma += v[i];
    validos++;
  }
  return validos ? soma / (float)validos : NAN;
}

void resumoFase(const __FlashStringHelper *fase, const float *pesos, const float *temps, byte n) {
  Serial.println(F("----------------------------------------"));
  Serial.print(F("RESUMO:")); Serial.println(fase);
  Serial.print(F("PONTOS:")); Serial.println(n);
  imprimir(F("PESO_INICIAL_G:"), pesos[0]);
  imprimir(F("PESO_FINAL_G:"), pesos[n - 1]);
  imprimir(F("DELTA_PESO_G:"), pesos[n - 1] - pesos[0]);
  imprimir(F("MEDIA_PESO_G:"), mediaIntervalo(pesos, 0, n - 1));
  float minima = NAN, maxima = NAN;
  byte validas = 0;
  for (byte i = 0; i < n; ++i) {
    if (isnan(temps[i])) continue;
    if (validas == 0 || temps[i] < minima) minima = temps[i];
    if (validas == 0 || temps[i] > maxima) maxima = temps[i];
    validas++;
  }
  Serial.print(F("TEMP_PONTOS_VALIDOS:")); Serial.println(validas);
  if (validas) {
    imprimir(F("TEMP_MEDIA_C:"), mediaIntervalo(temps, 0, n - 1));
    imprimir(F("TEMP_MIN_C:"), minima);
    imprimir(F("TEMP_MAX_C:"), maxima);
    imprimir(F("TEMP_VARIACAO_MAX_MIN_C:"), maxima - minima);
    if (!isnan(temps[0]) && !isnan(temps[n - 1])) {
      imprimir(F("TEMP_DELTA_INICIO_FIM_C:"), temps[n - 1] - temps[0]);
    }
  } else {
    Serial.println(F("TEMP_SEM_DADOS_VALIDOS"));
  }
}

void relatorio() {
  Serial.println(F("========================================"));
  Serial.println(F("RELATORIO_HX711_V10_PESO_E_TEMPERATURA"));
  Serial.println(F("PINO_NTC:A2; BETA:3950; R0:10000; R_FIXO:10000"));
  Serial.println(F("NTC_PARA_GND:SIM (SE MONTAGEM ORIGINAL)"));
  Serial.println(F("TARA_UNICA:SIM; PINOS_HX711:A1/A0; FATOR:-1129.6755"));
  Serial.println(F("INTERVALO_S:30; LEITURAS_POR_PONTO:24"));
  resumoFase(F("A_VAZIO_5MIN"), pesosA, tempA, PONTOS_A);
  resumoFase(F("B_CARGA_20MIN"), pesosB, tempB, PONTOS_B);
  resumoFase(F("C_RETORNO_ZERO_10MIN"), pesosC, tempC, PONTOS_C);
  imprimir(F("ZERO_ANTES_CARGA_G:"), pesosA[PONTOS_A - 1]);
  imprimir(F("ZERO_APOS_RETIRADA_INICIAL_G:"), pesosC[0]);
  imprimir(F("ZERO_APOS_RETIRADA_FINAL_G:"), pesosC[PONTOS_C - 1]);
  imprimir(F("RESIDUO_FINAL_VS_ZERO_ANTES_G:"), pesosC[PONTOS_C - 1] - pesosA[PONTOS_A - 1]);
  imprimir(F("PESO_CARGA_INICIAL_G:"), pesosB[0]);
  imprimir(F("PESO_CARGA_FINAL_G:"), pesosB[PONTOS_B - 1]);
  imprimir(F("ESTIMATIVA_EXPLORATORIA_CARGA_FINAL_MENOS_ZERO_APOS_G:"), pesosB[PONTOS_B - 1] - pesosC[0]);
  Serial.println(F("NOTA: ESTIMATIVA EXPLORATORIA NAO E CORRECAO VALIDADA."));
  Serial.println(F("NOTA: NTC mede temperatura local do sensor, nao necessariamente ambiente."));
  Serial.println(F("SEM_COMPENSACAO; SEM_NOVA_TARA; SEM_EEPROM; SEM_AQUECIMENTO"));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void setup() {
  configurarPinos();
  Serial.begin(9600);
  balanca.begin(PINO_DT, PINO_SCK);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("TESTE:HX711_V10_PESO_E_TEMPERATURA_5_20_10"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | Fator=-1129.6755"));
  Serial.println(F("FASES: A VAZIO 5MIN / B CARGA 20MIN / C ZERO 10MIN"));
  Serial.println(F("24 conversoes HX711 + 16 amostras NTC por ponto; intervalo 30s"));
  Serial.println(F("AVISO: D6/D11 LOW PODEM ATIVAR RELE ATIVO-BAIXO!"));
  Serial.println(F("AQUECEDOR PRECISA ESTAR SEM ALIMENTACAO DE POTENCIA."));
  Serial.println(F("NTC informa temperatura LOCAL DO SENSOR (nao necessariamente ambiente)."));
  Serial.println(F("Estabilize Arduino/HX711 energizados 30min antes do primeiro C."));

  float t, adc;
  if (lerNTC(t, adc)) {
    Serial.print(F("NTC_TESTE_ADC:")); Serial.print(adc, 1);
    Serial.print(F(":TEMP_NTC_C:")); Serial.println(t, 2);
  } else {
    Serial.print(F("AVISO:NTC_LEITURA_INVALIDA:ADC:")); Serial.println(adc, 1);
    Serial.println(F("Confira o divisor NTC antes de confiar na temperatura."));
  }

  if (!confirmar(F("FASE A: prato VAZIO, plataforma FIXA em X, aquecedor sem potencia?"))) return;
  Serial.println(F("HX711:DESCARTE_INICIAL:5"));
  for (byte i = 0; i < DESCARTES; ++i) {
    long adcHx;
    if (!lerADC(adcHx)) { Serial.println(F("TESTE:ERRO_DESCARTE")); return; }
  }
  Serial.println(F("TARA:REPOUSO_4S"));
  delay(4000);
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_TARA; ++i) {
    long adcHx;
    if (!lerADC(adcHx)) { Serial.println(F("TESTE:ERRO_TARA")); return; }
    soma += (int64_t)adcHx;
  }
  offsetADC = (long)(soma / LEITURAS_TARA);
  Serial.print(F("TARA:OK:OFFSET_ADC:")); Serial.println(offsetADC);

  Serial.println(F("FASE:A_VAZIO_5MIN"));
  delay(4000);
  if (!coletar(F("A_VAZIO"), PONTOS_A, pesosA, tempA)) {
    Serial.println(F("TESTE:ERRO_FASE_A")); return;
  }

  if (!confirmar(F("FASE B: coloque massa FIXA ~33,4 g no prato sem mover a estrutura."))) return;
  Serial.println(F("ACOMODACAO_CARGA:8S"));
  delay(8000);
  Medicao verificacao;
  if (!medir(verificacao)) { Serial.println(F("TESTE:ERRO_VALIDACAO_CARGA")); return; }
  Serial.print(F("VALIDACAO_CARGA_G:")); Serial.println(verificacao.peso, 4);
  if (verificacao.peso < 20.0f || verificacao.peso > 50.0f) {
    Serial.println(F("ERRO:CARGA_FORA_DA_FAIXA_20_A_50_G"));
    return;
  }
  Serial.println(F("FASE:B_CARGA_FIXA_20MIN"));
  if (!coletar(F("B_CARGA"), PONTOS_B, pesosB, tempB)) {
    Serial.println(F("TESTE:ERRO_FASE_B")); return;
  }

  if (!confirmar(F("FASE C: retire completamente a carga; prato metalico continua em X."))) return;
  Serial.println(F("ACOMODACAO_SEM_CARGA:8S"));
  delay(8000);
  Serial.println(F("FASE:C_RETORNO_ZERO_10MIN"));
  if (!coletar(F("C_ZERO"), PONTOS_C, pesosC, tempC)) {
    Serial.println(F("TESTE:ERRO_FASE_C")); return;
  }
  relatorio();
}

void loop() {
  manterAtuadoresParados();
  delay(100);
}
