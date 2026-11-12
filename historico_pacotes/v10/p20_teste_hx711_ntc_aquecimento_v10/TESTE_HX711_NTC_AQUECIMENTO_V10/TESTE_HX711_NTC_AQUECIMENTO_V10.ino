/*
 * TESTE_HX711_NTC_AQUECIMENTO_V10.ino
 * Arduino_Grao_CNN - Diagnostico ELETRICO dos comandos de aquecimento.
 *
 * VERSAO SEGURA / SEM AQUECIMENTO REAL:
 *  O programa alterna D6 e D11 para observar a influencia dos comandos
 *  dos reles na leitura HX711. A resistencia DEVE estar FISICAMENTE
 *  DESLIGADA da alimentacao de potencia durante TODO este ensaio.
 *
 * A: prato vazio, D6 e D11 em HIGH (5 pontos)
 * B: carga fixa ~36 g, D6 e D11 em HIGH (5 pontos)
 * C: carga, D6=LOW, D11=HIGH (5 pontos)
 * D: carga, D6=HIGH, D11=LOW (5 pontos)
 * E: carga, D6=HIGH, D11=HIGH (5 pontos)
 * F: prato vazio, D6=HIGH, D11=HIGH (5 pontos)
 *
 * AVISO SOBRE POLARIDADE:
 *  HIGH NAO SIGNIFICA NECESSARIAMENTE RELE DESLIGADO.
 *  Em placas ativas em HIGH, um rele pode ligar no inicio do sketch.
 *  O Arduino nao consegue garantir que a resistencia esteja sem energia:
 *  ISOLAR FISICAMENTE a alimentacao da resistencia ANTES do upload.
 *
 * A medicao NTC=A2 e da celula de carga e NAO controla nem protege
 * a temperatura da camara de aquecimento.
 * Nao existe acionamento de aquecimento real neste sketch.
 * Nenhuma gravacao em EEPROM. Firmware V10 principal inalterado.
 *
 * Bibliotecas: HX711 (bogde); Arduino IDE para placa Arduino Uno.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <limits.h>

const byte PINO_HX_DT = A1;
const byte PINO_HX_SCK = A0;
const byte PINO_NTC = A2;
const byte PINO_RELE_D6 = 6;
const byte PINO_RELE_D11 = 11;

// Calibracao experimental provisoria. Uma tara no inicio.
const float FATOR_CONTAGENS_POR_G = 1058.3167f;

const byte N_LEITURAS_TARA = 24;
const byte N_LEITURAS_PONTO = 24;
const byte N_AMOSTRAS_NTC = 16;
const byte N_PONTOS_POR_FASE = 5;
const byte N_DESCARTES = 5;
const unsigned long INTERVALO_PONTOS_MS = 30000UL;
const unsigned long TEMPO_ACOMODACAO_MS = 5000UL;
const unsigned long TIMEOUT_HX_MS = 3000UL;

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
  float soma;
  float primeiro;
  float ultimo;
  float tempSoma;
  float dpSoma;
  byte quantidade;
  byte temperaturasValidas;
};

// Prototipos explicitos para compatibilidade com o pre-processador da IDE.
void colocarSaidasEmHigh();
void cancelarTeste(const __FlashStringHelper *motivo);
void aguardarConfirmacao(char esperado, const __FlashStringHelper *aviso);
void esperarMs(unsigned long duracao);
void iniciarHX711();
void tararUmaVez();
bool obterTemperatura(float &temp);
bool obterMedicao(Medicao &m);
Resumo executarFase(const char *nome);
float mediaPeso(const Resumo &r);
void imprimirResumo(const char *nome, const Resumo &r);
void imprimirDelta(const __FlashStringHelper *rotulo, float delta);

void colocarSaidasEmHigh() {
  // HIGH e apenas um NIVEL LOGICO; a polaridade efetiva deve ser conferida.
  digitalWrite(PINO_RELE_D6, HIGH);
  digitalWrite(PINO_RELE_D11, HIGH);
}

void cancelarTeste(const __FlashStringHelper *motivo) {
  colocarSaidasEmHigh();
  Serial.print(F("TESTE:INTERROMPIDO:"));
  Serial.println(motivo);
  Serial.println(F("D6=D11=HIGH (nao garante rele desligado)."));
  while (true) delay(1000);
}

void aguardarConfirmacao(char esperado, const __FlashStringHelper *aviso) {
  Serial.println();
  Serial.println(aviso);
  Serial.print(F("Digite "));
  Serial.print(esperado);
  Serial.println(F(" + ENVIAR (Nova linha); S cancela."));

  while (true) {
    if (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
      if (c >= 'a' && c <= 'z') c -= ('a' - 'A');
      if (c == 'S') cancelarTeste(F("CANCELADO_PELO_USUARIO"));
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

void esperarMs(unsigned long duracao) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < duracao) delay(10);
}

void iniciarHX711() {
  balanca.begin(PINO_HX_DT, PINO_HX_SCK);
  for (byte i = 0; i < N_DESCARTES; i++) {
    if (!balanca.wait_ready_timeout(TIMEOUT_HX_MS))
      cancelarTeste(F("HX711_TIMEOUT_INICIAL"));
    (void)balanca.read();
  }
  Serial.println(F("HX711:INICIALIZADO"));
}

void tararUmaVez() {
  esperarMs(3000UL);
  double soma = 0.0;
  for (byte i = 0; i < N_LEITURAS_TARA; i++) {
    if (!balanca.wait_ready_timeout(TIMEOUT_HX_MS))
      cancelarTeste(F("HX711_TIMEOUT_TARA"));
    soma += balanca.read();
  }
  offsetADC = (long)round(soma / N_LEITURAS_TARA);
  Serial.print(F("TARA_UNICA:OFFSET_ADC:"));
  Serial.println(offsetADC);
}

bool obterTemperatura(float &temp) {
  float somaADC = 0.0f;
  for (byte i = 0; i < N_AMOSTRAS_NTC; i++) {
    (void)analogRead(PINO_NTC); // descartar primeira conversao apos MUX
    somaADC += analogRead(PINO_NTC);
    delay(2);
  }
  float adc = somaADC / (float)N_AMOSTRAS_NTC;
  if (adc <= 2.0f || adc >= 1021.0f) return false;

  float resistencia;
  if (NTC_PARA_GND)
    resistencia = NTC_R_FIXO * adc / (1023.0f - adc);
  else
    resistencia = NTC_R_FIXO * (1023.0f - adc) / adc;

  if (resistencia <= 0.0f) return false;
  float inversoT = (1.0f / NTC_T0_K) + log(resistencia / NTC_R0) / NTC_BETA;
  temp = (1.0f / inversoT) - 273.15f;
  return !isnan(temp) && !isinf(temp) && temp > -40.0f && temp < 150.0f;
}

bool obterMedicao(Medicao &m) {
  float mediaADC = 0.0f;
  float m2 = 0.0f;
  long menor = LONG_MAX;
  long maior = LONG_MIN;

  for (byte i = 0; i < N_LEITURAS_PONTO; i++) {
    if (!balanca.wait_ready_timeout(TIMEOUT_HX_MS)) return false;
    long valor = balanca.read();
    if (valor < menor) menor = valor;
    if (valor > maior) maior = valor;
    float diferenca = (float)valor - mediaADC;
    mediaADC += diferenca / (i + 1);
    m2 += diferenca * ((float)valor - mediaADC);
  }

  m.adc = mediaADC;
  m.peso = (mediaADC - (float)offsetADC) / FATOR_CONTAGENS_POR_G;
  m.dp = sqrt(m2 / (N_LEITURAS_PONTO - 1)) / FATOR_CONTAGENS_POR_G;
  m.amplitude = (float)(maior - menor) / FATOR_CONTAGENS_POR_G;
  m.tempValida = obterTemperatura(m.temperatura);
  return true;
}

Resumo executarFase(const char *nome) {
  Resumo r = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0};
  Serial.println();
  Serial.print(F("FASE:"));
  Serial.println(nome);

  unsigned long instanteInicial = millis();
  for (byte i = 0; i < N_PONTOS_POR_FASE; i++) {
    // A primeira medicao comeca imediatamente apos a acomodacao;
    // as demais sao programadas a cada 30 s desde o inicio da fase.
    unsigned long alvo = (unsigned long)i * INTERVALO_PONTOS_MS;
    while ((unsigned long)(millis() - instanteInicial) < alvo) delay(10);

    Medicao m;
    if (!obterMedicao(m)) cancelarTeste(F("HX711_TIMEOUT_MEDICAO"));
    if (i == 0) r.primeiro = m.peso;
    r.ultimo = m.peso;
    r.soma += m.peso;
    r.dpSoma += m.dp;
    r.quantidade++;
    if (m.tempValida) {
      r.tempSoma += m.temperatura;
      r.temperaturasValidas++;
    }

    Serial.print(F("PONTO:")); Serial.print(nome);
    Serial.print(F(":")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(N_PONTOS_POR_FASE);
    Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)i * (INTERVALO_PONTOS_MS / 1000UL));
    Serial.print(F(":ADC:")); Serial.print(m.adc, 1);
    Serial.print(F(":PESO_G:")); Serial.print(m.peso, 4);
    Serial.print(F(":DP_G:")); Serial.print(m.dp, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(m.amplitude, 4);
    Serial.print(F(":TEMP_C:"));
    if (m.tempValida) Serial.println(m.temperatura, 2);
    else Serial.println(F("INVALIDA"));
  }

  Serial.print(F("MEDIA_FASE:")); Serial.print(nome);
  Serial.print(F(":PESO_G:")); Serial.println(r.soma / r.quantidade, 4);
  return r;
}

float mediaPeso(const Resumo &r) {
  return r.quantidade ? r.soma / r.quantidade : NAN;
}

void imprimirResumo(const char *nome, const Resumo &r) {
  Serial.print(nome);
  Serial.print(F(":MEDIA_PESO_G:")); Serial.print(mediaPeso(r), 4);
  Serial.print(F(":DELTA_INICIO_FIM_G:")); Serial.print(r.ultimo - r.primeiro, 4);
  Serial.print(F(":DP_MEDIA_G:")); Serial.print(r.dpSoma / r.quantidade, 4);
  Serial.print(F(":TEMP_MEDIA_C:"));
  if (r.temperaturasValidas)
    Serial.println(r.tempSoma / r.temperaturasValidas, 2);
  else Serial.println(F("INVALIDA"));
}

void imprimirDelta(const __FlashStringHelper *rotulo, float delta) {
  Serial.print(rotulo);
  Serial.println(delta, 4);
}

void setup() {
  // Carregar HIGH no registrador antes de colocar pinos em OUTPUT.
  // NAO garante que sejam desligados em placas de rele ativas em HIGH!
  digitalWrite(PINO_RELE_D6, HIGH);
  digitalWrite(PINO_RELE_D11, HIGH);
  pinMode(PINO_RELE_D6, OUTPUT);
  pinMode(PINO_RELE_D11, OUTPUT);

  Serial.begin(9600);
  Serial.setTimeout(200);
  delay(300);
  Serial.println(F("TESTE_HX711_NTC_AQUECIMENTO_V10"));
  Serial.println(F("VERSAO:DIAGNOSTICO_RELES_SEM_AQUECIMENTO_REAL"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | D6/D11=RELES"));
  Serial.println(F("FATOR_PROVISORIO:1058.3167_CONTAGENS/G"));
  Serial.println(F("TARA_UNICA | 5 PONTOS/FASE | 24 LEITURAS/PONTO | 30 S"));
  Serial.println(F("CRITICO: RESISTENCIA DEVE ESTAR FISICAMENTE SEM POTENCIA."));
  Serial.println(F("HIGH/LOW SAO NIVEIS LOGICOS, NAO ESTADOS GARANTIDOS DOS RELES."));
  Serial.println(F("NTC NA CELULA NAO CONTROLA TEMPERATURA DA CAMARA."));
  Serial.println(F("SEM SERVO/MOTOR, SEM EEPROM, SEM ALTERAR V10 PRINCIPAL."));

  // Nao ha comutacao de reles antes da confirmacao de isolamento fisico,
  // mas os pinos comecam HIGH; em placa ativa em HIGH, relays podem ligar.
  aguardarConfirmacao('I', F("SEGURANCA: desconecte FISICAMENTE a alimentacao da RESISTENCIA.\nConfirme I SOMENTE apos garantir isolamento, inclusive ao reiniciar Arduino."));

  iniciarHX711();
  aguardarConfirmacao('C', F("A: prato vazio; nao toque na estrutura. Confirmar tara unica."));
  tararUmaVez();
  esperarMs(TEMPO_ACOMODACAO_MS);
  Resumo a = executarFase("A_VAZIO_D6_HIGH_D11_HIGH");

  aguardarConfirmacao('C', F("B: coloque a massa fixa ~36 g, sem alterar a estrutura."));
  esperarMs(TEMPO_ACOMODACAO_MS);
  Resumo b = executarFase("B_CARGA_D6_HIGH_D11_HIGH");

  aguardarConfirmacao('C', F("C: potencia da resistencia ISOLADA? C alterna D6 para LOW e mantem D11 HIGH."));
  digitalWrite(PINO_RELE_D11, HIGH);
  digitalWrite(PINO_RELE_D6, LOW);
  Serial.println(F("LOGICA_RELE:D6=LOW:D11=HIGH:SEM_POTENCIA_RESISTENCIA"));
  esperarMs(TEMPO_ACOMODACAO_MS);
  Resumo c = executarFase("C_CARGA_D6_LOW_D11_HIGH");

  aguardarConfirmacao('C', F("D: potencia da resistencia ISOLADA? C alterna D6 HIGH e D11 LOW."));
  digitalWrite(PINO_RELE_D6, HIGH);
  digitalWrite(PINO_RELE_D11, LOW);
  Serial.println(F("LOGICA_RELE:D6=HIGH:D11=LOW:SEM_POTENCIA_RESISTENCIA"));
  esperarMs(TEMPO_ACOMODACAO_MS);
  Resumo d = executarFase("D_CARGA_D6_HIGH_D11_LOW");

  aguardarConfirmacao('C', F("E: C retorna D6/D11 para HIGH (sem garantia de rele OFF)."));
  colocarSaidasEmHigh();
  Serial.println(F("LOGICA_RELE:D6=HIGH:D11=HIGH:SEM_POTENCIA_RESISTENCIA"));
  esperarMs(TEMPO_ACOMODACAO_MS);
  Resumo e = executarFase("E_CARGA_D6_HIGH_D11_HIGH");

  aguardarConfirmacao('C', F("F: retire totalmente a massa; deixe somente o prato metalico."));
  esperarMs(TEMPO_ACOMODACAO_MS);
  Resumo f = executarFase("F_VAZIO_FINAL");

  colocarSaidasEmHigh();
  Serial.println();
  Serial.println(F("========== RELATORIO FINAL =========="));
  imprimirResumo("A_VAZIO", a);
  imprimirResumo("B_CARGA_INICIAL", b);
  imprimirResumo("C_D6_LOW", c);
  imprimirResumo("D_D11_LOW", d);
  imprimirResumo("E_RELES_HIGH", e);
  imprimirResumo("F_VAZIO_FINAL", f);
  Serial.println(F("-------------------------------------"));
  imprimirDelta(F("DELTA_D6_LOW_MENOS_INICIAL_G:"), mediaPeso(c) - mediaPeso(b));
  imprimirDelta(F("DELTA_D11_LOW_MENOS_D6_LOW_G:"), mediaPeso(d) - mediaPeso(c));
  imprimirDelta(F("DELTA_RETORNO_HIGH_MENOS_D11_LOW_G:"), mediaPeso(e) - mediaPeso(d));
  imprimirDelta(F("DELTA_CARGA_FINAL_MENOS_INICIAL_G:"), mediaPeso(e) - mediaPeso(b));
  imprimirDelta(F("RESIDUO_ZERO_FINAL_MENOS_INICIAL_G:"), mediaPeso(f) - mediaPeso(a));
  Serial.println(F("NOTA: fases sequenciais nao provam causalidade eletrica isolada."));
  Serial.println(F("NOTA: nenhuma fase aquece a resistencia. NTC mede local da celula."));
  Serial.println(F("NOTA: HIGH pode ativar rele ativo-alto; mantenha resistencia isolada."));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void loop() {
  // Nenhuma acao automatica apos encerrar o teste.
  delay(1000);
}
