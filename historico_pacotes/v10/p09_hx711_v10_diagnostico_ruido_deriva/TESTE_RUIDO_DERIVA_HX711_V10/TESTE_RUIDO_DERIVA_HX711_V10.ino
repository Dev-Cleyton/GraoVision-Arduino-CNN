/*
 * TCC - Arduino_Grao_CNN, V10
 * TESTE_RUIDO_DERIVA_HX711_V10
 *
 * Diagnostico independente (NAO e o firmware principal de automacao).
 * A: prato metalico vazio em X, 10 min.
 * B: mesma bandeja com carga fixa, 10 min.
 * C: retirar carga e observar retorno ao zero.
 * Uma tara ao iniciar A, nunca nova tara entre A/B/C.
 * Sem movimento de servo ou motor, sem comando de aquecimento.
 *
 * ATENCAO: D6 e D11 sao mantidos LOW, tal como nos testes V10 anteriores.
 * LOW NAO e universalmente o estado desligado dos reles. Antes de rodar,
 * COMPROVE que o aquecedor nao pode energizar (corte de potencia seguro).
 * Driver de passo continua conectado; STEP permanece LOW.
 * O mecanismo vertical deve estar mecanicamente seguro em X.
 *
 * HX711: DT=A1, SCK=A0; fator provisiorio negativo: -1129.6755 cont/g.
 * Massa incluindo o prato deve ficar abaixo de 1 kg.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

const byte PINO_DT = A1;
const byte PINO_SCK = A0;
const byte PINO_DIR = 2;
const byte PINO_STEP = 3;
const byte PINO_RELE = 6;
const byte PINO_SERVO = 9;
const byte PINO_AQUECEDOR = 11;

const float FATOR_CONTAGENS_POR_G = -1129.6755f;
const unsigned long TIMEOUT_ADC_MS = 2500UL;
const unsigned long REPOUSO_MS = 4000UL;
const unsigned long INTERVALO_MS = 30000UL;
const byte TOTAL_PONTOS = 21;            // 0, 30, 60, ..., 600 s
const byte DESCARTE_INICIAL = 5;        // somente no inicio do ensaio
const byte LEITURAS_TARA = 20;
const byte LEITURAS_POR_PONTO = 24;
const float AMPLITUDE_AVISO_G = 0.30f;

HX711 balanca;
long offsetAdc = 0;

char comando[16];
byte comandoN = 0;

struct Medicao {
  float peso;
  float mediaADC;
  float amplitude;
  float desvioPadrao;
};

struct Fase {
  float primeiro;
  float ultimo;
  float minimo;
  float maximo;
  float soma;
  float somaDP;
  float maiorDP;
  float maiorAmplitude;
  float somaT;
  float somaT2;
  float somaTG;
  float inclinacao;
  byte qtd;
  byte alertasAmplitude;
};

Fase faseA;
Fase faseB;

void manterAtuadoresParados() {
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_DIR, LOW);
  digitalWrite(PINO_SERVO, LOW);
  // Niveis usados nos sketches V10 anteriores; validar circuito de rele.
  digitalWrite(PINO_RELE, LOW);
  digitalWrite(PINO_AQUECEDOR, LOW);
}

void configurarSaidas() {
  // Define os niveis antes de tornar os pinos saidas (sem pulsos STEP).
  manterAtuadoresParados();
  pinMode(PINO_STEP, OUTPUT);
  pinMode(PINO_DIR, OUTPUT);
  pinMode(PINO_SERVO, OUTPUT); // nao inclui Servo.h / sem PWM de servo
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_AQUECEDOR, OUTPUT);
  manterAtuadoresParados();
}

char lerComando() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      comando[comandoN] = '\0';
      comandoN = 0;
      if (strcmp(comando, "C") == 0 || strcmp(comando, "c") == 0) return 'C';
      if (strcmp(comando, "SAIR") == 0 || strcmp(comando, "sair") == 0) return 'S';
      if (comando[0] != '\0') Serial.println(F("COMANDO_INVALIDO: envie C ou SAIR com Nova linha."));
    } else if (comandoN < sizeof(comando)-1) {
      comando[comandoN++] = c;
    } else {
      comandoN = 0;
      Serial.println(F("COMANDO_LONGO_DEMAIS"));
    }
  }
  return '\0';
}

bool confirmar(const __FlashStringHelper* orientacao) {
  Serial.println();
  Serial.println(orientacao);
  Serial.println(F("Digite C + ENVIAR; SAIR cancela (Monitor Serial: Nova linha)."));
  while (true) {
    manterAtuadoresParados();
    const char resp = lerComando();
    if (resp == 'C') { Serial.println(F("CONFIRMADO")); return true; }
    if (resp == 'S') { Serial.println(F("TESTE:CANCELADO")); return false; }
    delay(20);
  }
}

bool aguardarMs(unsigned long intervalo) {
  unsigned long comeco = millis();
  while ((unsigned long)(millis() - comeco) < intervalo) {
    manterAtuadoresParados();
    if (lerComando() == 'S') {
      Serial.println(F("TESTE:CANCELADO"));
      return false;
    }
    delay(20);
  }
  return true;
}

bool lerADC(long &raw) {
  if (!balanca.wait_ready_timeout(TIMEOUT_ADC_MS)) {
    Serial.println(F("ERRO:HX711_SEM_RESPOSTA"));
    return false;
  }
  raw = balanca.read();
  return true;
}

bool descartarConversoes() {
  Serial.print(F("HX711:DESCARTE_INICIAL:"));
  Serial.println(DESCARTE_INICIAL);
  for (byte i = 0; i < DESCARTE_INICIAL; i++) {
    long bruto;
    if (!lerADC(bruto)) return false;
  }
  return true;
}

bool executarTaraUnica() {
  Serial.println(F("TARA:REPOUSO_4S"));
  if (!aguardarMs(REPOUSO_MS)) return false;
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_TARA; i++) {
    long bruto;
    if (!lerADC(bruto)) return false;
    soma += (int64_t)bruto;
  }
  offsetAdc = (long)(soma / LEITURAS_TARA);
  Serial.print(F("TARA:OK:OFFSET_ADC:")); Serial.println(offsetAdc);
  return true;
}

bool obterMedicao(Medicao &out) {
  // Welford: desvio-padrao estavel numericamente, sem vetor em RAM.
  float media = 0.0f;
  float m2 = 0.0f;
  float minG = 0.0f;
  float maxG = 0.0f;
  int64_t adcSoma = 0;

  for (byte i = 0; i < LEITURAS_POR_PONTO; i++) {
    long bruto;
    if (!lerADC(bruto)) return false;
    const float gramas = ((float)((int64_t)bruto - (int64_t)offsetAdc)) /
                          FATOR_CONTAGENS_POR_G;
    if (i == 0 || gramas < minG) minG = gramas;
    if (i == 0 || gramas > maxG) maxG = gramas;
    float delta = gramas - media;
    media += delta / (float)(i+1);
    float delta2 = gramas - media;
    m2 += delta * delta2;
    adcSoma += (int64_t)bruto;
  }
  out.peso = media;
  out.mediaADC = (float)adcSoma / (float)LEITURAS_POR_PONTO;
  out.amplitude = maxG - minG;
  out.desvioPadrao = sqrtf(m2 / (float)(LEITURAS_POR_PONTO - 1));
  return true;
}

void prepararFase(Fase &fase) {
  memset(&fase, 0, sizeof(Fase));
}

void acumular(Fase &fase, const Medicao &m, float tMin) {
  if (fase.qtd == 0) {
    fase.primeiro = m.peso;
    fase.minimo = m.peso;
    fase.maximo = m.peso;
  }
  if (m.peso < fase.minimo) fase.minimo = m.peso;
  if (m.peso > fase.maximo) fase.maximo = m.peso;
  if (m.desvioPadrao > fase.maiorDP) fase.maiorDP = m.desvioPadrao;
  if (m.amplitude > fase.maiorAmplitude) fase.maiorAmplitude = m.amplitude;
  if (m.amplitude > AMPLITUDE_AVISO_G) fase.alertasAmplitude++;
  fase.ultimo = m.peso;
  fase.qtd++;
  fase.soma += m.peso;
  fase.somaDP += m.desvioPadrao;
  fase.somaT += tMin;
  fase.somaT2 += tMin*tMin;
  fase.somaTG += tMin*m.peso;
}

void imprimirResumo(const __FlashStringHelper* nome, Fase &f) {
  const float n = (float)f.qtd;
  const float den = n*f.somaT2 - f.somaT*f.somaT;
  f.inclinacao = (fabsf(den) > 0.0001f) ?
                 (n*f.somaTG - f.somaT*f.soma)/den : 0.0f;
  Serial.println(F("----------------------------------------"));
  Serial.print(F("RESUMO:")); Serial.println(nome);
  Serial.print(F("PONTOS:")); Serial.println(f.qtd);
  Serial.print(F("PESO_INICIAL_G:")); Serial.println(f.primeiro, 4);
  Serial.print(F("PESO_FINAL_G:")); Serial.println(f.ultimo, 4);
  Serial.print(F("MEDIA_PONTOS_G:")); Serial.println(f.soma/n, 4);
  Serial.print(F("AMPLITUDE_ENTRE_PONTOS_G:")); Serial.println(f.maximo-f.minimo, 4);
  Serial.print(F("DERIVA_10_MIN_G:")); Serial.println(f.ultimo-f.primeiro, 4);
  Serial.print(F("TENDENCIA_G_POR_MIN:")); Serial.println(f.inclinacao, 5);
  Serial.print(F("RUIDO_MEDIO_DP_DENTRO_PONTO_G:")); Serial.println(f.somaDP/n, 4);
  Serial.print(F("MAIOR_DP_DENTRO_PONTO_G:")); Serial.println(f.maiorDP, 4);
  Serial.print(F("MAIOR_AMPLITUDE_DENTRO_PONTO_G:")); Serial.println(f.maiorAmplitude, 4);
  Serial.print(F("PONTOS_AMPLITUDE_ACIMA_0_30G:")); Serial.println(f.alertasAmplitude);
  Serial.println(F("NOTA: RUIDO e DERIVA sao indicadores diferentes."));
}

bool executarFase(const __FlashStringHelper* nome, Fase &f) {
  prepararFase(f);
  Serial.println(F("========================================"));
  Serial.print(F("FASE:")); Serial.println(nome);
  Serial.println(F("ACOMODACAO:4S"));
  if (!aguardarMs(REPOUSO_MS)) return false;
  const unsigned long inicio = millis();

  for (byte i = 0; i < TOTAL_PONTOS; ++i) {
    const unsigned long marco = (unsigned long)i * INTERVALO_MS;
    while ((unsigned long)(millis() - inicio) < marco) {
      manterAtuadoresParados();
      if (lerComando() == 'S') { Serial.println(F("TESTE:CANCELADO")); return false; }
      delay(20);
    }

    const unsigned long inicioMedicao = millis();
    Medicao p;
    if (!obterMedicao(p)) return false;
    const float tempoMin = (float)((unsigned long)(inicioMedicao - inicio)) / 60000.0f;
    acumular(f,p,tempoMin);

    Serial.print(F("PONTO:")); Serial.print(nome);
    Serial.print(F(":")); Serial.print(i+1);
    Serial.print(F("/")); Serial.print(TOTAL_PONTOS);
    Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)(inicioMedicao - inicio)/1000UL);
    Serial.print(F(":PESO_MEDIO_G:")); Serial.print(p.peso, 4);
    Serial.print(F(":RUIDO_DP_G:")); Serial.print(p.desvioPadrao, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(p.amplitude, 4);
    Serial.print(F(":ADC_MEDIO:")); Serial.print(p.mediaADC, 1);
    if (p.amplitude > AMPLITUDE_AVISO_G) Serial.print(F(":AVISO:OSCILACAO"));
    Serial.println();
  }

  imprimirResumo(nome,f);
  return true;
}

bool verificarZeroFinal() {
  Serial.println(F("========================================"));
  Serial.println(F("RETORNO_ZERO:ACOMODACAO:4S"));
  if (!aguardarMs(REPOUSO_MS)) return false;
  Medicao med;
  if (!obterMedicao(med)) return false;
  Serial.print(F("ZERO_FINAL_G:")); Serial.println(med.peso, 4);
  Serial.print(F("ZERO_FINAL_AMPLITUDE_G:")); Serial.println(med.amplitude, 4);
  Serial.print(F("ZERO_FINAL_MENOS_ZERO_FASE_A_INICIAL_G:"));
  Serial.println(med.peso - faseA.primeiro, 4);
  return true;
}

void finalizar(const __FlashStringHelper* motivo) {
  manterAtuadoresParados();
  Serial.print(F("TESTE:FINALIZADO:")); Serial.println(motivo);
}

void setup() {
  configurarSaidas();
  Serial.begin(9600);
  balanca.begin(PINO_DT,PINO_SCK);
  Serial.println(F("========================================"));
  Serial.println(F("TESTE:RUIDO_E_DERIVA_HX711_V10"));
  Serial.println(F("HX711 DT=A1, SCK=A0; fator -1129.6755 provisiorio"));
  Serial.println(F("Sem movimento, sem Servo.h; D6/D11 LOW."));
  Serial.println(F("AVISO: confirme rele e aquecedor fisicamente inativos."));
  Serial.println(F("NUNCA use HIGH/LOW sem conhecer polaridade do rele."));
  Serial.println(F("24 conversoes por ponto; 21 pontos em 10 minutos."));
  Serial.println(F("Ruido (DP local) e deriva (tendencia temporal) separados."));
  Serial.println(F("Uma tara com prato vazio em X; sem zerar peso negativo."));

  if (!confirmar(F("Bandeja VAZIA e FIXA em X; curso e aquecedor seguros?"))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!descartarConversoes()) { finalizar(F("ERRO_DESCARTE")); return; }
  if (!executarTaraUnica()) { finalizar(F("ERRO_TARA")); return; }
  if (!executarFase(F("A_VAZIO"),faseA)) { finalizar(F("ERRO_A")); return; }
  if (!confirmar(F("Coloque uma amostra FIXA (2 a 5 g) e mantenha a bandeja IMOVEL."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!executarFase(F("B_CARGA"),faseB)) { finalizar(F("ERRO_B")); return; }
  if (!confirmar(F("Retire os graos; deixe apenas o prato metalico em X."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!verificarZeroFinal()) { finalizar(F("ERRO_ZERO_FINAL")); return; }

  Serial.println(F("========================================"));
  Serial.println(F("RELATORIO_FINAL_RUIDO_DERIVA_V10"));
  Serial.print(F("DERIVA_ZERO_10_MIN_G:")); Serial.println(faseA.ultimo-faseA.primeiro, 4);
  Serial.print(F("DERIVA_CARGA_10_MIN_G:")); Serial.println(faseB.ultimo-faseB.primeiro, 4);
  Serial.print(F("RUIDO_MEDIO_VAZIO_DP_G:")); Serial.println(faseA.somaDP/faseA.qtd, 4);
  Serial.print(F("RUIDO_MEDIO_CARGA_DP_G:")); Serial.println(faseB.somaDP/faseB.qtd, 4);
  Serial.print(F("TENDENCIA_VAZIO_G_POR_MIN:")); Serial.println(faseA.inclinacao, 5);
  Serial.print(F("TENDENCIA_CARGA_G_POR_MIN:")); Serial.println(faseB.inclinacao, 5);
  Serial.println(F("SEM_AJUSTE_AUTOMATICO; SEM_EEPROM; SEM_AQUECIMENTO."));
  finalizar(F("SUCESSO"));
}

void loop() {
  manterAtuadoresParados();
  delay(100);
}
