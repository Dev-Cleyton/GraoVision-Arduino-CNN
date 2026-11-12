/*
 * TESTE_HX711_RETORNO_ZERO_CICLOS_V10.ino
 *
 * DIAGNOSTICO: 2 ciclos consecutivos de carga e retirada SEM NOVA TARA.
 * Cada ciclo: 5 min com massa fixa ~36 g e 5 min com prato vazio.
 * Referencia inicial: prato vazio por 2 min, apos tara unica.
 * 24 leituras do HX711, 16 leituras NTC e 30 s por ponto.
 *
 * Ligacoes ja validadas no TCC:
 *  HX711 DT=A1, SCK=A0 | NTC=A2
 *  Rele1 IN1=D7 (aquecedor), Rele2 IN2=D6 (driver)
 *  Rele3 IN3=D8 (reserva),  Rele4 IN4=D4 (reserva).
 *  Os 4 reles permanecem DESLIGADOS (HIGH) durante TODO o ensaio.
 *
 * SEGURANCA: SEM CARGAS NOS CONTATOS COM/NO/NC DOS RELES!
 * Aquecedor e driver sem qualquer potencia externa.
 * O reset/boot pode ativar transitoriamente reles antes do setup.
 * O NTC perto da celula NAO e protecao da camara de aquecimento.
 * O servo e motor NAO se movimentam neste codigo.
 * Nao modifica EEPROM nem firmware principal V10.
 *
 * Requer a mesma biblioteca HX711 (bogde) dos testes anteriores.
 * Serial: 9600 baud, "Nova linha". C confirma, S cancela.
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

const byte RELE_1 = 7; // IN1 aquecedor (so acionamento logico)
const byte RELE_2 = 6; // IN2 driver (so acionamento logico)
const byte RELE_3 = 8; // IN3 reserva
const byte RELE_4 = 4; // IN4 reserva

const float FATOR_CONTAGENS_POR_G = 1058.3167f;
const float NTC_R0_OHMS = 10000.0f;
const float NTC_R_FIXO_OHMS = 10000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_T0_K = 298.15f;
const bool NTC_PARA_GND = true;

const unsigned long INTERVALO_MS = 30000UL;
const unsigned long TIMEOUT_HX_MS = 2500UL;
const byte DESCARTES = 5;
const byte LEITURAS_TARA = 20;
const byte LEITURAS_PONTO = 24;
const byte AMOSTRAS_NTC = 16;

const byte PONTOS_BASE = 5; // 0, 30, 60, 90, 120 s
const byte PONTOS_FASE = 11; // 0 ... 300 s => 5 minutos

HX711 balanca;
long offsetADC = 0;

float pesosBase[PONTOS_BASE], tempBase[PONTOS_BASE];
float pesosCarga1[PONTOS_FASE], tempCarga1[PONTOS_FASE];
float pesosZero1[PONTOS_FASE], tempZero1[PONTOS_FASE];
float pesosCarga2[PONTOS_FASE], tempCarga2[PONTOS_FASE];
float pesosZero2[PONTOS_FASE], tempZero2[PONTOS_FASE];

// Declarar a estrutura ANTES dos prototipos impede o erro:
// "'Medicao' does not name a type" no pre-processador Arduino IDE.
struct Medicao {
  float peso;
  float dp;
  float amplitude;
  float adcMedio;
  float tempC;
  float ntcADC;
  bool ntcValido;
};

// Prototipos explicitos (compativel com Arduino IDE e Arduino Uno).
void configurarReleOff(byte pino);
void manterRelesOff();
void manterAtuadoresParados();
void configurarPinos();
void verificarAbortar();
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
bool fazerTaraUnica();
bool medirCarga(const __FlashStringHelper *fase, float *pesos, float *temps);
bool medirRetornoZero(const __FlashStringHelper *fase, float *pesos, float *temps);
void relatorio();
void setup();
void loop();

void configurarReleOff(byte pino) {
  digitalWrite(pino, HIGH);
  pinMode(pino, OUTPUT);
}

void manterRelesOff() {
  digitalWrite(RELE_1, HIGH);
  digitalWrite(RELE_2, HIGH);
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);
}

void manterAtuadoresParados() {
  digitalWrite(PINO_DIR, LOW);
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_SERVO, LOW);
  manterRelesOff();
}

void configurarPinos() {
  // Configure PRIMEIRO os reles ativos em LOW para sairem em HIGH.
  configurarReleOff(RELE_1);
  configurarReleOff(RELE_2);
  configurarReleOff(RELE_3);
  configurarReleOff(RELE_4);
  digitalWrite(PINO_DIR, LOW);
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_SERVO, LOW);
  pinMode(PINO_DIR, OUTPUT);
  pinMode(PINO_STEP, OUTPUT);
  pinMode(PINO_SERVO, OUTPUT);
  pinMode(PINO_NTC, INPUT);
  manterAtuadoresParados();
}

// Permite interromper a fase com 'S' sem energizar os reles.
void verificarAbortar() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 'S' || c == 's') {
      manterRelesOff();
      Serial.println(F("TESTE:ABORTADO_PELO_USUARIO"));
      Serial.println(F("RELES:COMANDADOS_OFF:HIGH"));
      while (true) { manterRelesOff(); delay(100); }
    }
  }
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
        if (n == 1 && (linha[0] == 'S' || linha[0] == 's')) return 'S';
        if (strcmp(linha, "SAIR") == 0 || strcmp(linha, "sair") == 0) return 'S';
        if (n > 0) Serial.println(F("COMANDO_INVALIDO: C ou S"));
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
    verificarAbortar();
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
      verificarAbortar();
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


bool fazerTaraUnica() {
  Serial.println(F("HX711:DESCARTE_INICIAL:5"));
  for (byte i = 0; i < DESCARTES; ++i) {
    verificarAbortar();
    long adc;
    if (!lerADC(adc)) return false;
  }

  Serial.println(F("TARA:REPOUSO_4S"));
  delay(4000);
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_TARA; ++i) {
    verificarAbortar();
    long adc;
    if (!lerADC(adc)) return false;
    soma += (int64_t)adc;
  }
  offsetADC = (long)(soma / (int64_t)LEITURAS_TARA);
  Serial.print(F("TARA_UNICA:OK:OFFSET_ADC:"));
  Serial.println(offsetADC);
  return true;
}

bool medirCarga(const __FlashStringHelper *fase, float *pesos, float *temps) {
  if (!confirmar(F("Coloque a massa FIXA ~36 g, centralizada. Nao mova a plataforma.")))
    return false;

  Serial.println(F("ACOMODACAO_CARGA:8S"));
  delay(8000);
  Medicao verificacao;
  if (!medir(verificacao)) return false;
  Serial.print(F("VALIDACAO_CARGA_G:"));
  Serial.println(verificacao.peso, 4);
  if (verificacao.peso < 20.0f || verificacao.peso > 50.0f) {
    Serial.println(F("ERRO:CARGA_FORA_DA_FAIXA_20_A_50_G"));
    return false;
  }

  Serial.print(F("FASE:")); Serial.println(fase);
  return coletar(fase, PONTOS_FASE, pesos, temps);
}

bool medirRetornoZero(const __FlashStringHelper *fase, float *pesos, float *temps) {
  if (!confirmar(F("RETIRE completamente a massa. Mantenha o prato e a plataforma fixos.")))
    return false;

  Serial.println(F("ACOMODACAO_SEM_CARGA:8S"));
  delay(8000);

  Serial.print(F("FASE:")); Serial.println(fase);
  return coletar(fase, PONTOS_FASE, pesos, temps);
}

void relatorio() {
  manterRelesOff();
  const float zeroAntesC1 = pesosBase[PONTOS_BASE - 1];
  const float zeroAntesC2 = pesosZero1[PONTOS_FASE - 1];
  const float zeroFinalC1 = pesosZero1[PONTOS_FASE - 1];
  const float zeroFinalC2 = pesosZero2[PONTOS_FASE - 1];

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F("RELATORIO_HX711_RETORNO_ZERO_CICLOS_V10"));
  Serial.println(F("HX711:A1/A0; NTC:A2; FATOR:1058.3167"));
  Serial.println(F("TARA_UNICA:SIM; 2_CICLOS_5MIN_CARGA_5MIN_ZERO"));
  Serial.println(F("PONTOS_CARGA:11; PONTOS_ZERO:11; INTERVALO_S:30"));
  Serial.println(F("RELES:IN1_D7=HIGH:IN2_D6=HIGH:IN3_D8=HIGH:IN4_D4=HIGH"));
  Serial.println(F("RELES_CONECTADOS_MAS_SEMPRE_OFF; SEM_CARGAS_DE_POTENCIA"));
  Serial.print(F("OFFSET_TARA_ADC:")); Serial.println(offsetADC);

  resumoFase(F("A_VAZIO_BASE_2MIN"), pesosBase, tempBase, PONTOS_BASE);
  resumoFase(F("B1_CARGA_5MIN"), pesosCarga1, tempCarga1, PONTOS_FASE);
  resumoFase(F("C1_ZERO_5MIN"), pesosZero1, tempZero1, PONTOS_FASE);
  resumoFase(F("B2_CARGA_5MIN"), pesosCarga2, tempCarga2, PONTOS_FASE);
  resumoFase(F("C2_ZERO_5MIN"), pesosZero2, tempZero2, PONTOS_FASE);

  Serial.println(F("---------- COMPARATIVO DOS CICLOS ----------"));
  imprimir(F("ZERO_ANTES_CICLO1_G:"), zeroAntesC1);
  imprimir(F("ZERO_APOS_RETIRADA_C1_INICIAL_G:"), pesosZero1[0]);
  imprimir(F("ZERO_APOS_RETIRADA_C1_FINAL_G:"), zeroFinalC1);
  imprimir(F("RESIDUO_C1_INICIAL_VS_ZERO_ANTES_G:"), pesosZero1[0] - zeroAntesC1);
  imprimir(F("RESIDUO_C1_FINAL_VS_ZERO_ANTES_G:"), zeroFinalC1 - zeroAntesC1);

  imprimir(F("ZERO_ANTES_CICLO2_G:"), zeroAntesC2);
  imprimir(F("ZERO_APOS_RETIRADA_C2_INICIAL_G:"), pesosZero2[0]);
  imprimir(F("ZERO_APOS_RETIRADA_C2_FINAL_G:"), zeroFinalC2);
  imprimir(F("RESIDUO_C2_INICIAL_VS_ZERO_ANTES_G:"), pesosZero2[0] - zeroAntesC2);
  imprimir(F("RESIDUO_C2_FINAL_VS_ZERO_ANTES_G:"), zeroFinalC2 - zeroAntesC2);
  imprimir(F("RESIDUO_FINAL_C2_VS_BASE_ORIGINAL_G:"), zeroFinalC2 - zeroAntesC1);

  imprimir(F("CARGA_C1_INICIAL_G:"), pesosCarga1[0]);
  imprimir(F("CARGA_C1_FINAL_G:"), pesosCarga1[PONTOS_FASE - 1]);
  imprimir(F("CARGA_C2_INICIAL_G:"), pesosCarga2[0]);
  imprimir(F("CARGA_C2_FINAL_G:"), pesosCarga2[PONTOS_FASE - 1]);
  imprimir(F("DELTA_MEDIA_CARGA_C2_MENOS_C1_G:"),
          mediaIntervalo(pesosCarga2, 0, PONTOS_FASE - 1)
        - mediaIntervalo(pesosCarga1, 0, PONTOS_FASE - 1));

  Serial.println(F("NOTA: DELTAS INCLUEM DERIVA TEMPORAL E POSSIVEL HISTERESE."));
  Serial.println(F("NOTA: LEITURA DE ZERO APOS CARGA NAO E CORRECAO VALIDADA."));
  Serial.println(F("NOTA: NTC MEDE TEMPERATURA LOCAL, NAO DA CAMARA."));
  Serial.println(F("SEM_COMPENSACAO; SEM_NOVA_TARA; SEM_EEPROM; SEM_AQUECIMENTO"));
  Serial.println(F("RELES:COMANDADOS_OFF:HIGH"));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
  Serial.println(F("=========================================="));
}

void setup() {
  configurarPinos();  // Precarrega HIGH antes de OUTPUT para relés ativos em LOW.
  Serial.begin(9600);
  balanca.begin(PINO_DT, PINO_SCK);

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F("TESTE_HX711_RETORNO_ZERO_CICLOS_V10"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | FATOR=1058.3167"));
  Serial.println(F("IN1=D7 IN2=D6 IN3=D8 IN4=D4; LOW=LIGA"));
  Serial.println(F("TODOS OS RELES PERMANECEM OFF=HIGH"));
  Serial.println(F("UMA TARA; BASE VAZIO 2MIN; DOIS CICLOS 5MIN+5MIN"));
  Serial.println(F("24 LEITURAS HX711 + 16 NTC; 30S ENTRE PONTOS"));
  Serial.println(F("ATENCAO: CONTATOS COM/NO/NC SEM CARGAS DE POTENCIA."));
  Serial.println(F("AQUECEDOR E DRIVER SEM ALIMENTACAO DE POTENCIA."));
  Serial.println(F("RESET PODE ACIONAR RELES ANTES DO setup()."));
  Serial.println(F("NTC NA CELULA NAO PROTEGE A CAMARA DE AQUECIMENTO."));
  Serial.println(F("MONITOR SERIAL: 9600 BAUD / NOVA LINHA / S ABORTA."));
  Serial.println(F("ESTABILIZE ARDUINO E HX711 POR 30 MIN ANTES DE CONFIRMAR."));

  float temperaturaTeste, adcTeste;
  if (lerNTC(temperaturaTeste, adcTeste)) {
    Serial.print(F("NTC_TESTE_ADC:")); Serial.print(adcTeste, 1);
    Serial.print(F(":TEMP_NTC_C:")); Serial.println(temperaturaTeste, 2);
  } else {
    Serial.print(F("AVISO:NTC_INVALIDO:ADC:")); Serial.println(adcTeste, 1);
  }

  if (!confirmar(F("CONFIRME: relés conectados, mas SEM CARGAS; prato vazio e plataforma fixa.")))
    return;

  Serial.println(F("RELES:IN1_D7=HIGH:IN2_D6=HIGH:IN3_D8=HIGH:IN4_D4=HIGH"));
  if (!fazerTaraUnica()) { Serial.println(F("TESTE:ERRO_TARA")); return; }

  Serial.println(F("FASE:A_VAZIO_BASE_2MIN"));
  if (!coletar(F("A_VAZIO"), PONTOS_BASE, pesosBase, tempBase)) {
    Serial.println(F("TESTE:ERRO_BASE")); return;
  }

  Serial.println(F("CICLO:1/2"));
  if (!medirCarga(F("B1_CARGA"), pesosCarga1, tempCarga1)) {
    Serial.println(F("TESTE:ERRO_CARGA_C1_OU_CANCELADO")); return;
  }
  if (!medirRetornoZero(F("C1_ZERO"), pesosZero1, tempZero1)) {
    Serial.println(F("TESTE:ERRO_ZERO_C1_OU_CANCELADO")); return;
  }

  Serial.println(F("CICLO:2/2"));
  if (!medirCarga(F("B2_CARGA"), pesosCarga2, tempCarga2)) {
    Serial.println(F("TESTE:ERRO_CARGA_C2_OU_CANCELADO")); return;
  }
  if (!medirRetornoZero(F("C2_ZERO"), pesosZero2, tempZero2)) {
    Serial.println(F("TESTE:ERRO_ZERO_C2_OU_CANCELADO")); return;
  }

  relatorio();
}

void loop() {
  manterAtuadoresParados();
  delay(100);
}
