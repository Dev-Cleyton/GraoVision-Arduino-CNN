/*
 * TESTE_HX711_NTC_AQUECIMENTO_V10_V2.ino
 * Projeto Arduino_Grao_CNN — Arduino Uno + Screw Shield 1.0
 *
 * ENSAIO DE INTERFERENCIA DOS COMANDOS DOS RELES NO HX711/NTC.
 * ESTE PROGRAMA NAO REALIZA AQUECIMENTO REAL.
 *
 * Rele 1 (IN1) = D7 = reservado ao AQUECEDOR
 * Rele 2 (IN2) = D6 = reservado a ALIMENTACAO DO DRIVER DE PASSO
 * Rele 3 (IN3) = D8 = RESERVA (permanece OFF)
 * Rele 4 (IN4) = D4 = RESERVA (permanece OFF)
 * Modulo testado em bancada: LOW=LIGA, HIGH=DESLIGA.
 * HX711: DT=A1, SCK=A0; NTC=A2, junto a celula de carga.
 * Fator experimental: +1058.3167 contagens/grama.
 *
 * ETAPAS (5 pontos/fase, separados por 30 segundos):
 * A: prato vazio, todos reles OFF -> tara unica.
 * B: carga fixa ~36 g, todos OFF.
 * C: carga fixa, SOMENTE rele 1 (D7) ON — SEM POTENCIA.
 * D: carga fixa, SOMENTE rele 2 (D6) ON — SEM POTENCIA.
 * E: carga fixa, todos OFF.
 * F: retirar carga, todos OFF; verificar retorno ao zero.
 *
 * SEGURANCA OBRIGATORIA:
 * 1. DESCONECTE FISICAMENTE TODOS OS CABOS DE POTENCIA DOS CONTATOS
 *    COM / NO / NC DOS QUATRO RELES, ANTES DE INSTALAR O SKETCH.
 * 2. SEM REDE ELETRICA NA RESISTENCIA; SEM FONTE DO DRIVER NOS CONTATOS.
 * 3. Nao ligue quaisquer cargas de potencia durante o ensaio.
 * 4. No reset/boot do Arduino, os GPIOs ficam temporariamente em alta
 *    impedancia; o software NAO garante ausencia de acionamento nesse momento.
 * 5. O NTC junto a celula NAO substitui um sensor/limitador da camara.
 * 6. Este sketch NAO controla motor, servo, EEPROM nem a V10 principal.
 * 7. Envie 'S' no monitor para abortar e comandar todos relés OFF.
 *
 * Requer biblioteca HX711 de Bogde (API: begin, wait_ready_timeout, read).
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <limits.h>

const uint8_t PINO_HX_DT = A1;
const uint8_t PINO_HX_SCK = A0;
const uint8_t PINO_NTC = A2;
const uint8_t PINOS_RELE[4] = {7, 6, 8, 4};
const uint8_t RELE_LIGA = LOW;
const uint8_t RELE_DESLIGA = HIGH;

const float FATOR_CONTAGENS_POR_G = 1058.3167f;
const uint8_t N_TARA = 24;
const uint8_t N_LEITURAS = 24;
const uint8_t N_NTC = 16;
const uint8_t N_PONTOS = 5;
const unsigned long ENTRE_PONTOS_MS = 30000UL;
const unsigned long ACOMODACAO_MS = 5000UL;
const unsigned long HX_TIMEOUT_MS = 3000UL;

const float NTC_R0 = 10000.0f;
const float NTC_R_FIXO = 10000.0f;
const float NTC_BETA = 3950.0f;
const float NTC_T0_K = 298.15f;
const bool NTC_PARA_GND = true;

HX711 balanca;
long offsetADC = 0;

struct Medicao {
  float adc;
  float peso;
  float dp;
  float amplitude;
  float temperatura;
  bool tempValida;
};

struct Resumo {
  float somaPeso;
  float primeiro;
  float ultimo;
  float somaDP;
  float somaTemp;
  uint8_t total;
  uint8_t totalTemp;
};

// Prototipos explicitos: evita erros do pre-processador da Arduino IDE.
void desligarTodos();
void ligarSomente(uint8_t indice);
void cancelar(const __FlashStringHelper *motivo);
void verificarCancelamento();
void aguardarComando(char esperado, const __FlashStringHelper *instrucao);
void esperarComCancelamento(unsigned long tempoMs);
void iniciarHX();
void fazerTara();
bool lerTemperatura(float &temperatura);
bool lerMedicao(Medicao &m);
Resumo medirFase(const char *nome);
float media(const Resumo &r);
void relatarFase(const char *nome, const Resumo &r);

void desligarTodos() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(PINOS_RELE[i], RELE_DESLIGA);
}

void ligarSomente(uint8_t indice) {
  desligarTodos();
  if (indice < 4) digitalWrite(PINOS_RELE[indice], RELE_LIGA);
}

void cancelar(const __FlashStringHelper *motivo) {
  desligarTodos();
  Serial.print(F("TESTE:INTERROMPIDO:"));
  Serial.println(motivo);
  Serial.println(F("RELES:COMANDADOS_OFF:HIGH"));
  // Nao reacionar automaticamente nenhum rele apos falha ou cancelamento.
  while (true) delay(1000);
}

// 'S' cancela mesmo durante as medicoes ou temporizacoes.
void verificarCancelamento() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 's' || c == 'S') cancelar(F("CANCELADO_POR_SERIAL"));
  }
}

void aguardarComando(char esperado, const __FlashStringHelper *instrucao) {
  // Evita aceitar caracteres antigos de uma etapa anterior.
  while (Serial.available() > 0) (void)Serial.read();
  Serial.println();
  Serial.println(instrucao);
  Serial.print(F("Digite "));
  Serial.print(esperado);
  Serial.println(F(" + ENVIAR (Nova linha). S cancela."));

  while (true) {
    if (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
      if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
      if (c == 'S') cancelar(F("CANCELADO_POR_USUARIO"));
      if (c == esperado) {
        while (Serial.available() > 0) (void)Serial.read();
        Serial.println(F("CONFIRMADO"));
        return;
      }
      Serial.println(F("COMANDO_INVALIDO"));
    }
    delay(10);
  }
}

void esperarComCancelamento(unsigned long tempoMs) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < tempoMs) {
    verificarCancelamento();
    delay(10);
  }
}

void iniciarHX() {
  balanca.begin(PINO_HX_DT, PINO_HX_SCK);
  for (uint8_t i = 0; i < 5; i++) {
    verificarCancelamento();
    if (!balanca.wait_ready_timeout(HX_TIMEOUT_MS)) cancelar(F("HX711_TIMEOUT_INICIAL"));
    (void)balanca.read();
  }
  Serial.println(F("HX711:INICIALIZADO"));
}

void fazerTara() {
  esperarComCancelamento(3000UL);
  double acumulado = 0.0;
  for (uint8_t i = 0; i < N_TARA; i++) {
    verificarCancelamento();
    if (!balanca.wait_ready_timeout(HX_TIMEOUT_MS)) cancelar(F("HX711_TIMEOUT_TARA"));
    acumulado += balanca.read();
  }
  offsetADC = (long)round(acumulado / N_TARA);
  Serial.print(F("TARA_UNICA:OFFSET_ADC:"));
  Serial.println(offsetADC);
}

bool lerTemperatura(float &temperatura) {
  float somaADC = 0.0f;
  for (uint8_t i = 0; i < N_NTC; i++) {
    verificarCancelamento();
    (void)analogRead(PINO_NTC);
    somaADC += analogRead(PINO_NTC);
    delay(2);
  }
  float adc = somaADC / (float)N_NTC;
  if (adc <= 2.0f || adc >= 1021.0f) return false;
  float r = NTC_PARA_GND ? (NTC_R_FIXO * adc / (1023.0f - adc))
                        : (NTC_R_FIXO * (1023.0f - adc) / adc);
  if (r <= 0.0f) return false;
  float invT = (1.0f / NTC_T0_K) + log(r / NTC_R0) / NTC_BETA;
  temperatura = 1.0f / invT - 273.15f;
  return !isnan(temperatura) && !isinf(temperatura) && temperatura > -40.0f && temperatura < 150.0f;
}

bool lerMedicao(Medicao &m) {
  // Welford: calcula desvio padrao de cada grupo de 24 conversoes.
  float mediaADC = 0.0f;
  float m2 = 0.0f;
  long menor = LONG_MAX;
  long maior = LONG_MIN;

  for (uint8_t i = 0; i < N_LEITURAS; i++) {
    verificarCancelamento();
    if (!balanca.wait_ready_timeout(HX_TIMEOUT_MS)) return false;
    long adc = balanca.read();
    if (adc < menor) menor = adc;
    if (adc > maior) maior = adc;
    float delta = (float)adc - mediaADC;
    mediaADC += delta / (i + 1);
    m2 += delta * ((float)adc - mediaADC);
  }
  m.adc = mediaADC;
  m.peso = (mediaADC - (float)offsetADC) / FATOR_CONTAGENS_POR_G;
  m.dp = sqrt(m2 / (N_LEITURAS - 1)) / FATOR_CONTAGENS_POR_G;
  m.amplitude = (float)(maior - menor) / FATOR_CONTAGENS_POR_G;
  m.tempValida = lerTemperatura(m.temperatura);
  return true;
}

Resumo medirFase(const char *nome) {
  Resumo r = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0};
  Serial.println();
  Serial.print(F("FASE:")); Serial.println(nome);
  unsigned long inicio = millis();

  for (uint8_t i = 0; i < N_PONTOS; i++) {
    unsigned long alvo = (unsigned long)i * ENTRE_PONTOS_MS;
    while ((unsigned long)(millis() - inicio) < alvo) {
      verificarCancelamento();
      delay(10);
    }
    Medicao m;
    if (!lerMedicao(m)) cancelar(F("HX711_TIMEOUT_MEDICAO"));
    if (i == 0) r.primeiro = m.peso;
    r.ultimo = m.peso;
    r.somaPeso += m.peso;
    r.somaDP += m.dp;
    r.total++;
    if (m.tempValida) {
      r.somaTemp += m.temperatura;
      r.totalTemp++;
    }
    Serial.print(F("PONTO:")); Serial.print(nome);
    Serial.print(F(":")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(N_PONTOS);
    Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)i * (ENTRE_PONTOS_MS / 1000UL));
    Serial.print(F(":ADC:")); Serial.print(m.adc, 1);
    Serial.print(F(":PESO_G:")); Serial.print(m.peso, 4);
    Serial.print(F(":DP_G:")); Serial.print(m.dp, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(m.amplitude, 4);
    Serial.print(F(":TEMP_C:"));
    if (m.tempValida) Serial.println(m.temperatura, 2);
    else Serial.println(F("INVALIDA"));
  }
  Serial.print(F("MEDIA_FASE:")); Serial.print(nome);
  Serial.print(F(":PESO_G:")); Serial.println(r.somaPeso / r.total, 4);
  return r;
}

float media(const Resumo &r) { return r.total ? r.somaPeso / r.total : 0.0f; }

void relatarFase(const char *nome, const Resumo &r) {
  Serial.print(nome);
  Serial.print(F(":MEDIA_PESO_G:")); Serial.print(media(r), 4);
  Serial.print(F(":DELTA_INICIO_FIM_G:")); Serial.print(r.ultimo - r.primeiro, 4);
  Serial.print(F(":DP_MEDIA_G:")); Serial.print(r.somaDP / r.total, 4);
  Serial.print(F(":TEMP_MEDIA_C:"));
  if (r.totalTemp) Serial.println(r.somaTemp / r.totalTemp, 2);
  else Serial.println(F("INVALIDA"));
}

void setup() {
  // Modulo RELÉ V2 confirmado ativo em LOW; primeiro deixa latch HIGH,
  // depois configura saidas. Reset/boot fora do controle do programa.
  for (uint8_t i = 0; i < 4; i++) digitalWrite(PINOS_RELE[i], RELE_DESLIGA);
  for (uint8_t i = 0; i < 4; i++) pinMode(PINOS_RELE[i], OUTPUT);
  desligarTodos();

  Serial.begin(9600);
  Serial.setTimeout(200);
  delay(300);
  Serial.println(F("TESTE_HX711_NTC_AQUECIMENTO_V10_V2"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | FATOR=1058.3167"));
  Serial.println(F("RELE1 D7=AQUECEDOR | RELE2 D6=DRIVER"));
  Serial.println(F("RELE3 D8=RESERVA | RELE4 D4=RESERVA"));
  Serial.println(F("LOGICA TESTADA: LOW=LIGA HIGH=DESLIGA"));
  Serial.println(F("TARA UNICA | 5 PONTOS/FASE | 24 HX711 | 16 NTC | 30S"));
  Serial.println(F("ATENCAO: SEM CARGAS NOS CONTATOS COM/NO/NC!"));
  Serial.println(F("AQUECEDOR E DRIVER DEVEM ESTAR SEM POTENCIA!"));
  Serial.println(F("O NTC NA CELULA NAO PROTEGE A CAMARA DE AQUECIMENTO."));
  Serial.println(F("ENVIE S PARA ABORTAR, QUANDO NECESSARIO."));
  Serial.println(F("SEM SERVO, SEM MOTOR, SEM EEPROM, V10 PRINCIPAL INALTERADA."));
  Serial.println(F("=========================================="));

  aguardarComando('I', F("CONFIRME QUE TODOS OS CONTATOS DOS RELES ESTAO SEM CARGAS DE POTENCIA."));
  iniciarHX();

  aguardarComando('C', F("A: PRATO VAZIO, TODOS RELES OFF. Confirmar para tara unica."));
  fazerTara();
  esperarComCancelamento(ACOMODACAO_MS);
  Resumo a = medirFase("A_VAZIO_TODOS_OFF");

  aguardarComando('C', F("B: coloque massa fixa ~36 g, sem mover a estrutura. TODOS OFF."));
  esperarComCancelamento(ACOMODACAO_MS);
  Resumo b = medirFase("B_CARGA_TODOS_OFF");

  aguardarComando('C', F("C: LIGAR SOMENTE RELE 1 D7 (AQUECEDOR SEM POTENCIA)."));
  ligarSomente(0);
  Serial.println(F("LOGICA_RELE:IN1_D7=LOW:IN2_D6=HIGH:IN3_D8=HIGH:IN4_D4=HIGH"));
  esperarComCancelamento(ACOMODACAO_MS);
  Resumo c = medirFase("C_CARGA_RELE1_ON");
  desligarTodos();  // Mantem ambos OFF enquanto espera nova confirmacao.

  aguardarComando('C', F("D: LIGAR SOMENTE RELE 2 D6 (DRIVER SEM POTENCIA)."));
  ligarSomente(1);
  Serial.println(F("LOGICA_RELE:IN1_D7=HIGH:IN2_D6=LOW:IN3_D8=HIGH:IN4_D4=HIGH"));
  esperarComCancelamento(ACOMODACAO_MS);
  Resumo d = medirFase("D_CARGA_RELE2_ON");
  desligarTodos();

  aguardarComando('C', F("E: confirmar TODOS RELES OFF, carga mantida no prato."));
  desligarTodos();
  esperarComCancelamento(ACOMODACAO_MS);
  Resumo e = medirFase("E_CARGA_TODOS_OFF");

  aguardarComando('C', F("F: retirar massa e deixar somente prato metalico, TODOS OFF."));
  desligarTodos();
  esperarComCancelamento(ACOMODACAO_MS);
  Resumo f = medirFase("F_VAZIO_FINAL");

  desligarTodos();
  Serial.println();
  Serial.println(F("========== RELATORIO FINAL =========="));
  relatarFase("A_VAZIO", a);
  relatarFase("B_CARGA_INICIAL", b);
  relatarFase("C_RELE1_AQUECEDOR_LOGICO_ON", c);
  relatarFase("D_RELE2_DRIVER_LOGICO_ON", d);
  relatarFase("E_CARGA_FINAL_TODOS_OFF", e);
  relatarFase("F_VAZIO_FINAL", f);
  Serial.println(F("-------------------------------------"));
  Serial.print(F("DELTA_RELE1_ON_MENOS_CARGA_INICIAL_G:")); Serial.println(media(c) - media(b), 4);
  Serial.print(F("DELTA_RELE2_ON_MENOS_CARGA_INICIAL_G:")); Serial.println(media(d) - media(b), 4);
  Serial.print(F("DELTA_CARGA_FINAL_MENOS_INICIAL_G:")); Serial.println(media(e) - media(b), 4);
  Serial.print(F("RESIDUO_ZERO_FINAL_MENOS_INICIAL_G:")); Serial.println(media(f) - media(a), 4);
  Serial.println(F("NOTA: COMPARACOES SUJEITAS A DERIVA TEMPORAL."));
  Serial.println(F("NOTA: NAO TESTA AQUECIMENTO REAL OU ENERGIZACAO DO MOTOR."));
  Serial.println(F("RELES:COMANDADOS_OFF:HIGH"));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void loop() {
  // Ensaio de execucao unica. Nenhum acionamento automatico apos concluir.
  desligarTodos();
  delay(1000);
}
