/*
  TESTE_HX711_RELES_DERIVA_COMPARATIVO_V10.ino
  Arduino Uno / Screw Shield / HX711 / NTC / modulo de 4 reles

  OBJETIVO: investigar deriva temporal e efeito do acionamento LOGICO
  dos reles sobre a leitura de uma massa fixa, com controles OFF intercalados.

  IMPORTANTE - SEGURANCA:
  - Desconecte FISICAMENTE todas as cargas de potencia dos contatos
    COM/NO/NC. Nao ha aquecimento nem alimentacao do motor neste teste.
  - Saidas ativas em LOW. Na inicializacao/reset os pinos podem flutuar;
    o firmware nao substitui protecoes eletricas/fisicas.
  - NTC A2 esta proximo a celula; NAO monitora a camara de aquecimento.
  - Nao alterar o firmware principal V10; nao usa EEPROM.
  - Se necessario, envie S + Nova linha para abortar.

  PINAGEM:
  HX711 DT=A1  SCK=A0; NTC=A2
  RELE1 IN1=D7 (aquecedor, somente comando logico)
  RELE2 IN2=D6 (driver, somente comando logico)
  RELE3 IN3=D8 (reserva, mantido OFF)
  RELE4 IN4=D4 (reserva, mantido OFF)

  METODO:
  - tara unica, com prato vazio.
  - cinco pontos/etapa, a cada 30s; 24 conversoes HX711 por ponto.
  - 16 amostras NTC por ponto.
  - carga fixa ~36g permanece no prato em todas as 9 etapas centrais.
  - sequencia automatica OFF / R1 / OFF / R2 / OFF / R2 / OFF / R1 / OFF.
  - efeito exploratorio: ON - media dos dois controles OFF vizinhos.
  - iniciar teste apenas com as cargas de potencia desconectadas.
*/

#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// Pinos confirmados na montagem V10
const uint8_t HX_DOUT = A1;
const uint8_t HX_SCK = A0;
const uint8_t NTC_PIN = A2;
const uint8_t RELE_1_PIN = 7;
const uint8_t RELE_2_PIN = 6;
const uint8_t RELE_3_PIN = 8;
const uint8_t RELE_4_PIN = 4;

const float FATOR_CONTAGENS_POR_G = 1058.3167f; // Provisorio, positivo
const uint8_t HX_LEITURAS = 24;
const uint8_t NTC_LEITURAS = 16;
const uint8_t PONTOS_POR_FASE = 5;
const unsigned long INTERVALO_PONTOS_MS = 30000UL;
const unsigned long ESTABILIZACAO_FASE_MS = 8000UL;
const unsigned long HX_TIMEOUT_MS = 1500UL;

// Divisor NTC: resistor fixo entre 5V e A2; NTC entre A2 e GND
const float NTC_R_FIXO = 10000.0f;
const float NTC_R_25 = 10000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_T_25_K = 298.15f;

// 0 = todos OFF, 1 = rele 1 ligado, 2 = rele 2 ligado.
enum ModoRele : uint8_t { TODOS_OFF = 0, RELE1_ON = 1, RELE2_ON = 2 };

struct Fase {
  const char *nome;
  ModoRele modo;
};

const Fase fases[] = {
  {"VAZIO_INICIAL", TODOS_OFF},  // 0
  {"OFF_A", TODOS_OFF},         // 1
  {"RELE1_ON_A", RELE1_ON},     // 2
  {"OFF_B", TODOS_OFF},         // 3
  {"RELE2_ON_A", RELE2_ON},     // 4
  {"OFF_C", TODOS_OFF},         // 5
  {"RELE2_ON_B", RELE2_ON},     // 6
  {"OFF_D", TODOS_OFF},         // 7
  {"RELE1_ON_B", RELE1_ON},     // 8
  {"OFF_E", TODOS_OFF},         // 9
  {"VAZIO_FINAL", TODOS_OFF}    // 10
};
const uint8_t NUM_FASES = sizeof(fases) / sizeof(fases[0]);

struct ResumoFase {
  float mediaG;
  float primeiraG;
  float ultimaG;
  float mediaTempC;
  uint8_t nTempValidas;
};
ResumoFase resumos[NUM_FASES];

long offsetADC = 0;
bool autorizado = false;

// Primeiro fixa HIGH no latch de saida, e SO ENTAO muda pinMode.
// O Arduino nao garante estado OFF durante a alimentacao/reset.
void configurarReleDesligado(uint8_t pino) {
  digitalWrite(pino, HIGH);
  pinMode(pino, OUTPUT);
}

void todosRelesOff() {
  digitalWrite(RELE_1_PIN, HIGH);
  digitalWrite(RELE_2_PIN, HIGH);
  digitalWrite(RELE_3_PIN, HIGH);
  digitalWrite(RELE_4_PIN, HIGH);
}

void aplicarModoRele(ModoRele modo) {
  todosRelesOff();
  if (modo == RELE1_ON) digitalWrite(RELE_1_PIN, LOW);
  if (modo == RELE2_ON) digitalWrite(RELE_2_PIN, LOW);
  // Reles 3 e 4 permanecem desligados em todo o ensaio.
}

void pararComSeguranca(const __FlashStringHelper *motivo) {
  todosRelesOff();
  Serial.println();
  Serial.print(F("TESTE:INTERROMPIDO:"));
  Serial.println(motivo);
  Serial.println(F("RELES:COMANDADOS_OFF:HIGH"));
  while (true) { delay(100); } // Pressione RESET para reiniciar.
}

void verificarCancelamento() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 'S' || c == 's') {
      pararComSeguranca(F("CANCELADO_PELO_USUARIO"));
    }
  }
}

void esperarComAbort(unsigned long duracaoMs) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < duracaoMs) {
    verificarCancelamento();
    delay(5);
  }
}

void esperarAte(unsigned long instanteMs) {
  // Comparacao robusta ao overflow do millis(), contanto que intervalos < 2^31 ms.
  while ((long)(millis() - instanteMs) < 0) {
    verificarCancelamento();
    delay(5);
  }
}

void confirmar(const __FlashStringHelper *instrucao) {
  todosRelesOff();
  Serial.println();
  Serial.println(instrucao);
  Serial.println(F("Digite C + ENVIAR (Nova linha); S cancela."));
  while (true) {
    if (Serial.available()) {
      char c = (char)Serial.read();
      if (c == 'S' || c == 's') pararComSeguranca(F("CANCELADO_PELO_USUARIO"));
      if (c == 'C' || c == 'c') {
        Serial.println(F("CONFIRMADO"));
        // Descarte CR/LF que porventura ja esteja no buffer.
        while (Serial.available()) Serial.read();
        return;
      }
    }
    delay(10);
  }
}

// Leitura bruta sem biblioteca externa. HX711 no canal A, ganho 128.
long lerHX711Bruto() {
  unsigned long inicio = millis();
  while (digitalRead(HX_DOUT) == HIGH) {
    verificarCancelamento();
    if ((unsigned long)(millis() - inicio) > HX_TIMEOUT_MS) {
      pararComSeguranca(F("HX711_TIMEOUT_VERIFIQUE_DT_SCK"));
    }
    delay(1);
  }

  uint32_t valor = 0;
  noInterrupts();
  for (uint8_t i = 0; i < 24; ++i) {
    digitalWrite(HX_SCK, HIGH);
    delayMicroseconds(2);
    valor = (valor << 1) | (digitalRead(HX_DOUT) ? 1UL : 0UL);
    digitalWrite(HX_SCK, LOW);
    delayMicroseconds(2);
  }
  // Pulso 25: proxima conversao = canal A, ganho 128.
  digitalWrite(HX_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX_SCK, LOW);
  delayMicroseconds(2);
  interrupts();

  // HX711 fornece inteiro com sinal de 24 bits em complemento de dois.
  if (valor & 0x800000UL) valor |= 0xFF000000UL;
  return (long)((int32_t)valor);
}

struct LeituraPeso {
  float adcMedio;
  float pesoG;
  float dpG;
  float amplitudeG;
};

LeituraPeso medirPeso() {
  double soma = 0.0;
  double media = 0.0;
  double somaQuadrados = 0.0;
  long minimo = 2147483647L;
  long maximo = (-2147483647L - 1L);

  for (uint8_t i = 0; i < HX_LEITURAS; i++) {
    verificarCancelamento();
    long adc = lerHX711Bruto();
    soma += adc;
    if (adc < minimo) minimo = adc;
    if (adc > maximo) maximo = adc;
    double delta = (double)adc - media;
    media += delta / (double)(i + 1);
    somaQuadrados += delta * ((double)adc - media);
  }
  LeituraPeso r;
  r.adcMedio = (float)(soma / (double)HX_LEITURAS);
  r.pesoG = (r.adcMedio - (float)offsetADC) / FATOR_CONTAGENS_POR_G;
  r.dpG = (float)sqrt(somaQuadrados / (double)(HX_LEITURAS - 1)) / FATOR_CONTAGENS_POR_G;
  r.amplitudeG = (float)(maximo - minimo) / FATOR_CONTAGENS_POR_G;
  return r;
}

bool lerTemperaturaC(float &temperatura) {
  float somaADC = 0.0f;
  for (uint8_t i = 0; i < NTC_LEITURAS; ++i) {
    verificarCancelamento();
    somaADC += analogRead(NTC_PIN);
    delay(3);
  }
  float adc = somaADC / (float)NTC_LEITURAS;
  if (adc <= 2.0f || adc >= 1021.0f) return false;
  float rNTC = NTC_R_FIXO * adc / (1023.0f - adc);
  float inversoT = (1.0f / NTC_T_25_K) + (log(rNTC / NTC_R_25) / NTC_BETA);
  if (inversoT <= 0.0f) return false;
  temperatura = (1.0f / inversoT) - 273.15f;
  return temperatura >= -20.0f && temperatura <= 125.0f;
}

void fazerTaraUnica() {
  Serial.println(F("TARA:REPOUSO_4S"));
  esperarComAbort(4000);
  double soma = 0.0;
  for (uint8_t i = 0; i < HX_LEITURAS; ++i) {
    verificarCancelamento();
    soma += lerHX711Bruto();
  }
  offsetADC = (long)(soma / (double)HX_LEITURAS);
  Serial.print(F("TARA_UNICA:OFFSET_ADC:"));
  Serial.println(offsetADC);
}

void executarFase(uint8_t indice) {
  const Fase &fase = fases[indice];
  aplicarModoRele(fase.modo);
  Serial.println();
  Serial.print(F("FASE:")); Serial.println(fase.nome);
  Serial.print(F("RELES:IN1_D7=")); Serial.print(fase.modo == RELE1_ON ? F("LOW") : F("HIGH"));
  Serial.print(F(":IN2_D6=")); Serial.print(fase.modo == RELE2_ON ? F("LOW") : F("HIGH"));
  Serial.println(F(":IN3_D8=HIGH:IN4_D4=HIGH"));

  esperarComAbort(ESTABILIZACAO_FASE_MS);
  unsigned long inicio = millis();
  float somaG = 0.0f;
  float somaTemp = 0.0f;
  uint8_t nTemp = 0;

  for (uint8_t i = 0; i < PONTOS_POR_FASE; ++i) {
    esperarAte(inicio + (unsigned long)i * INTERVALO_PONTOS_MS);
    LeituraPeso p = medirPeso();
    float temp = 0.0f;
    bool tempOk = lerTemperaturaC(temp);

    if (i == 0) resumos[indice].primeiraG = p.pesoG;
    if (i == PONTOS_POR_FASE - 1) resumos[indice].ultimaG = p.pesoG;
    somaG += p.pesoG;
    if (tempOk) { somaTemp += temp; ++nTemp; }

    Serial.print(F("PONTO:")); Serial.print(fase.nome);
    Serial.print(F(":")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(PONTOS_POR_FASE);
    Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)i * INTERVALO_PONTOS_MS / 1000UL);
    Serial.print(F(":ADC:")); Serial.print(p.adcMedio, 1);
    Serial.print(F(":PESO_G:")); Serial.print(p.pesoG, 4);
    Serial.print(F(":DP_G:")); Serial.print(p.dpG, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(p.amplitudeG, 4);
    Serial.print(F(":TEMP_C:"));
    if (tempOk) Serial.println(temp, 2); else Serial.println(F("INVALIDA"));
  }

  // Mantem fase ate completar os cinco intervalos de 30s (150s totais).
  esperarAte(inicio + (unsigned long)PONTOS_POR_FASE * INTERVALO_PONTOS_MS);
  resumos[indice].mediaG = somaG / (float)PONTOS_POR_FASE;
  resumos[indice].mediaTempC = nTemp ? (somaTemp / (float)nTemp) : 0.0f;
  resumos[indice].nTempValidas = nTemp;

  Serial.print(F("MEDIA_FASE:")); Serial.print(fase.nome);
  Serial.print(F(":PESO_G:")); Serial.print(resumos[indice].mediaG, 4);
  Serial.print(F(":DELTA_G:"));
  Serial.print(resumos[indice].ultimaG - resumos[indice].primeiraG, 4);
  Serial.print(F(":TEMP_MEDIA_C:"));
  if (nTemp) Serial.println(resumos[indice].mediaTempC, 2);
  else Serial.println(F("INVALIDA"));
}

void imprimirEfeito(const __FlashStringHelper *rotulo, uint8_t idxON,
                    uint8_t idxOFFAntes, uint8_t idxOFFDepois) {
  float referencia = (resumos[idxOFFAntes].mediaG + resumos[idxOFFDepois].mediaG) * 0.5f;
  float delta = resumos[idxON].mediaG - referencia;
  Serial.print(rotulo); Serial.print(F(":ON_MENOS_OFF_INTERPOLADO_G:"));
  Serial.println(delta, 4);
}

void relatorioFinal() {
  todosRelesOff();
  Serial.println();
  Serial.println(F("========== RELATORIO_FINAL_COMPARATIVO_V10 =========="));
  Serial.println(F("MAPA:IN1_D7:IN2_D6:IN3_D8:IN4_D4"));
  Serial.print(F("TARA_UNICA_ADC:")); Serial.println(offsetADC);
  Serial.print(F("FATOR_PROVISORIO:")); Serial.println(FATOR_CONTAGENS_POR_G, 4);
  Serial.println(F("CARGA_FIXA:SEM_RETIRADA_ENTRE_OFF_A_E_OFF_E"));
  for (uint8_t i = 0; i < NUM_FASES; i++) {
    Serial.print(F("RESUMO:")); Serial.print(fases[i].nome);
    Serial.print(F(":MEDIA_G:")); Serial.print(resumos[i].mediaG, 4);
    Serial.print(F(":DELTA_INICIO_FIM_G:"));
    Serial.print(resumos[i].ultimaG - resumos[i].primeiraG, 4);
    Serial.print(F(":TEMP_MEDIA_C:"));
    if (resumos[i].nTempValidas) Serial.println(resumos[i].mediaTempC, 2);
    else Serial.println(F("INVALIDA"));
  }
  Serial.println(F("-------------------------------------"));
  imprimirEfeito(F("EFEITO_RELE1_REP1"), 2, 1, 3);
  imprimirEfeito(F("EFEITO_RELE2_REP1"), 4, 3, 5);
  imprimirEfeito(F("EFEITO_RELE2_REP2"), 6, 5, 7);
  imprimirEfeito(F("EFEITO_RELE1_REP2"), 8, 7, 9);
  Serial.print(F("DELTA_CARGA_FINAL_VS_INICIAL_G:"));
  Serial.println(resumos[9].mediaG - resumos[1].mediaG, 4);
  Serial.print(F("RESIDUO_ZERO_FINAL_VS_INICIAL_G:"));
  Serial.println(resumos[10].mediaG - resumos[0].mediaG, 4);
  Serial.println(F("NOTA: ON - OFF INTERPOLADO E ESTIMATIVA EXPLORATORIA."));
  Serial.println(F("NOTA: NAO PROVA CAUSALIDADE; AVALIAR TENDENCIA/TEMPERATURA."));
  Serial.println(F("NOTA: NAO HOUVE AQUECIMENTO NEM MOTOR ENERGIZADO."));
  Serial.println(F("RELES:COMANDADOS_OFF:HIGH"));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void setup() {
  // Prioridade: configurar os quatro controles de rele em OFF.
  configurarReleDesligado(RELE_1_PIN);
  configurarReleDesligado(RELE_2_PIN);
  configurarReleDesligado(RELE_3_PIN);
  configurarReleDesligado(RELE_4_PIN);
  todosRelesOff();

  digitalWrite(HX_SCK, LOW);
  pinMode(HX_SCK, OUTPUT);
  pinMode(HX_DOUT, INPUT);
  pinMode(NTC_PIN, INPUT);

  Serial.begin(9600);
  Serial.println();
  Serial.println(F("TESTE_HX711_RELES_DERIVA_COMPARATIVO_V10"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | FATOR=1058.3167"));
  Serial.println(F("RELE1 D7=AQUECEDOR | RELE2 D6=DRIVER"));
  Serial.println(F("RELE3 D8=RESERVA | RELE4 D4=RESERVA"));
  Serial.println(F("LOGICA:LOW=LIGA HIGH=DESLIGA"));
  Serial.println(F("CARGA FIXA ~36G / TARA UNICA / 5 PONTOS POR FASE / 30S"));
  Serial.println(F("SEQUENCIA:OFF-R1-OFF-R2-OFF-R2-OFF-R1-OFF"));
  Serial.println(F("AQUECEDOR/DRIVER SEM POTENCIA; CONTATOS SEM CARGAS!"));
  Serial.println(F("RELES PODEM ACIONAR DURANTE RESET: NAO USE CARGAS CONECTADAS!"));
  Serial.println(F("SEM SERVO, SEM MOTOR, SEM EEPROM, V10 INALTERADA."));
  Serial.println(F("ENVIE S PARA INTERROMPER; MONITOR 9600, NOVA LINHA."));

  confirmar(F("CONFIRME QUE TODOS COM/NO/NC ESTAO SEM CARGAS DE POTENCIA."));
  autorizado = true;
  if (!autorizado) pararComSeguranca(F("NAO_AUTORIZADO"));
  confirmar(F("A: Prato VAZIO. Plataforma fixa. Confirmar TARA unica."));
  fazerTaraUnica();
  executarFase(0);

  confirmar(F("B: Coloque massa FIXA ~36g, NAO RETIRE ate o fim de OFF_E."));
  Serial.println(F("INICIANDO 9 FASES AUTOMATICAS: NAO TOQUE NA BALANCA."));
  for (uint8_t i = 1; i <= 9; ++i) {
    executarFase(i);
  }
  todosRelesOff();
  confirmar(F("F: Agora RETIRE completamente a massa; mantenha prato metalico."));
  executarFase(10);
  relatorioFinal();
}

void loop() {
  todosRelesOff();
  verificarCancelamento();
  delay(250);
}
