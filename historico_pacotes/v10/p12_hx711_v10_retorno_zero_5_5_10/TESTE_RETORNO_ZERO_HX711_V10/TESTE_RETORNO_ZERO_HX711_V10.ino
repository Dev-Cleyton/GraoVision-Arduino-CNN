/*
  Arduino_Grao_CNN / TCC - Ensaio diagnostico HX711 V10
  TESTE_RETORNO_ZERO_HX711_V10

  ETAPA A: bandeja vazia em X, 5 minutos (11 pontos).
  ETAPA B: mesma bandeja com carga fixa ~33,4 g, 5 minutos (11 pontos).
  ETAPA C: retirar carga, observar retorno ao zero por 10 minutos (21 pontos).
  Uma tara inicial somente; nao zera nas transicoes entre fases.
  24 conversoes por ponto, intervalo entre inicios de pontos de 30 s.
  Gera tendencia, dispersao, residuo de zero e recuperacao aproximada.

  SEGURANCA: o sketch NAO controla aquecimento nem o movimento.
  Os pinos D6/D11 permanecem LOW, tal como o sketch diagnostico V10
  anterior. LOW PODE LIGAR um rele ativo em nivel baixo: somente execute
  depois de garantir que a fonte de potencia do aquecedor esta DESLIGADA
  por meio seguro (sem alterar fios de sinal). Confirme que a plataforma
  esta imovel e que o motor nao se movera. STEP fica LOW; sem Servo.h.

  HX711 DT=A1 SCK=A0 (ligacao validada anteriormente), fator PROVISORIO.
  Sem compensacao de deriva, sem mudancas em EEPROM ou firmware principal.
  MASSA TOTAL (prato + carga) deve ficar abaixo de 1 kg.
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
const byte PONTOS_5_MIN = 11;   // t=0,30,...,300 segundos
const byte PONTOS_10_MIN = 21;  // t=0,30,...,600 segundos
const byte DESCARTE_INICIAL = 5;
const byte LEITURAS_TARA = 20;
const byte LEITURAS_POR_PONTO = 24;
const float AMPLITUDE_AVISO_G = 0.30f;
const float LIMIAR_RECUPERACAO_G = 0.05f;

HX711 balanca;
long offsetAdc = 0;
char comando[16];
byte comandoN = 0;

struct Medicao {
  float peso;
  float mediaADC;
  float amplitude;
  float dp;
};

struct Fase {
  float primeiro, ultimo, minimo, maximo;
  float soma, somaDP, maiorDP, maiorAmplitude;
  float somaT, somaT2, somaTG, inclinacao;
  float somaPrimeiros3, somaUltimos3;
  byte qtd, alertasAmplitude;
};

Fase faseA, faseB, faseC;

// Resultado de tres pontos consecutivos de C dentro de +/-0,05 g de
// A_FINAL. Comparacao aproximada, NAO e correcao nem garantia metrologica.
byte consecutivosRecuperacao = 0;
long primeiroTempoRecuperacaoS = -1;

void manterAtuadoresParados() {
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_DIR, LOW);
  digitalWrite(PINO_SERVO, LOW);  // sem PWM
  digitalWrite(PINO_RELE, LOW);   // requer aquecedor SEM potencia!
  digitalWrite(PINO_AQUECEDOR, LOW); // requer aquecedor SEM potencia!
}

void configurarSaidas() {
  manterAtuadoresParados();
  pinMode(PINO_STEP, OUTPUT);
  pinMode(PINO_DIR, OUTPUT);
  pinMode(PINO_SERVO, OUTPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_AQUECEDOR, OUTPUT);
  manterAtuadoresParados();
}

char lerComando() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      comando[comandoN] = '\0';
      comandoN = 0;
      if (strcmp(comando, "C") == 0 || strcmp(comando, "c") == 0) return 'C';
      if (strcmp(comando, "SAIR") == 0 || strcmp(comando, "sair") == 0) return 'S';
      if (comando[0]) Serial.println(F("COMANDO_INVALIDO: use C ou SAIR com Nova linha"));
    } else if (comandoN < sizeof(comando) - 1) {
      comando[comandoN++] = c;
    } else {
      comandoN = 0;
      Serial.println(F("COMANDO_LONGO_DEMAIS"));
    }
  }
  return '\0';
}

bool confirmar(const __FlashStringHelper *instrucoes) {
  Serial.println();
  Serial.println(instrucoes);
  Serial.println(F("Digite C e clique ENVIAR (Monitor Serial: Nova linha)."));
  Serial.println(F("Digite SAIR para cancelar."));
  while (true) {
    manterAtuadoresParados();
    char r = lerComando();
    if (r == 'C') {
      Serial.println(F("CONFIRMADO"));
      return true;
    }
    if (r == 'S') {
      Serial.println(F("TESTE:CANCELADO"));
      return false;
    }
    delay(20);
  }
}

bool aguardarMs(unsigned long duracao) {
  const unsigned long t0 = millis();
  while ((unsigned long)(millis() - t0) < duracao) {
    manterAtuadoresParados();
    if (lerComando() == 'S') {
      Serial.println(F("TESTE:CANCELADO"));
      return false;
    }
    delay(20);
  }
  return true;
}

bool lerADC(long &bruto) {
  if (!balanca.wait_ready_timeout(TIMEOUT_ADC_MS)) {
    Serial.println(F("ERRO:HX711_SEM_RESPOSTA"));
    return false;
  }
  bruto = balanca.read();
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

bool realizarTaraUnica() {
  Serial.println(F("TARA:REPOUSO_4S"));
  if (!aguardarMs(REPOUSO_MS)) return false;
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_TARA; i++) {
    long bruto;
    if (!lerADC(bruto)) return false;
    soma += (int64_t)bruto;
  }
  offsetAdc = (long)(soma / LEITURAS_TARA);
  Serial.print(F("TARA:OK:OFFSET_ADC:"));
  Serial.println(offsetAdc);
  return true;
}

bool medir(Medicao &out) {
  float media = 0.0f, m2 = 0.0f, minimo = 0.0f, maximo = 0.0f;
  int64_t somaADC = 0;
  for (byte i = 0; i < LEITURAS_POR_PONTO; i++) {
    long bruto;
    if (!lerADC(bruto)) return false;
    const float g = (float)((int64_t)bruto - (int64_t)offsetAdc) /
                    FATOR_CONTAGENS_POR_G;
    if (i == 0 || g < minimo) minimo = g;
    if (i == 0 || g > maximo) maximo = g;
    const float delta = g - media;
    media += delta / (float)(i + 1);
    m2 += delta * (g - media);
    somaADC += (int64_t)bruto;
  }
  out.peso = media;
  out.mediaADC = (float)somaADC / LEITURAS_POR_PONTO;
  out.amplitude = maximo - minimo;
  out.dp = sqrtf(m2 / (LEITURAS_POR_PONTO - 1));
  return true;
}

void zerarFase(Fase &f) {
  memset(&f, 0, sizeof(f));
}

void acumular(Fase &f, const Medicao &m, float tMin, byte indice, byte total) {
  if (f.qtd == 0) {
    f.primeiro = m.peso;
    f.minimo = m.peso;
    f.maximo = m.peso;
  }
  if (m.peso < f.minimo) f.minimo = m.peso;
  if (m.peso > f.maximo) f.maximo = m.peso;
  if (m.dp > f.maiorDP) f.maiorDP = m.dp;
  if (m.amplitude > f.maiorAmplitude) f.maiorAmplitude = m.amplitude;
  if (m.amplitude > AMPLITUDE_AVISO_G) f.alertasAmplitude++;
  if (indice < 3) f.somaPrimeiros3 += m.peso;
  if (indice >= total - 3) f.somaUltimos3 += m.peso;
  f.ultimo = m.peso;
  f.qtd++;
  f.soma += m.peso;
  f.somaDP += m.dp;
  f.somaT += tMin;
  f.somaT2 += tMin*tMin;
  f.somaTG += tMin*m.peso;
}

void imprimirResumo(const __FlashStringHelper *nome, Fase &f, byte duracaoMin) {
  const float n = (float)f.qtd;
  const float den = n*f.somaT2 - f.somaT*f.somaT;
  f.inclinacao = (fabsf(den) > 0.0001f) ?
                (n*f.somaTG - f.somaT*f.soma)/den : 0.0f;
  Serial.println(F("----------------------------------------"));
  Serial.print(F("RESUMO:")); Serial.println(nome);
  Serial.print(F("PONTOS:")); Serial.println(f.qtd);
  Serial.print(F("PESO_INICIAL_G:")); Serial.println(f.primeiro, 4);
  Serial.print(F("PESO_FINAL_G:")); Serial.println(f.ultimo, 4);
  Serial.print(F("DERIVA_FASE_G:")); Serial.println(f.ultimo - f.primeiro, 4);
  Serial.print(F("DURACAO_MIN:")); Serial.println(duracaoMin);
  Serial.print(F("TENDENCIA_G_POR_MIN:")); Serial.println(f.inclinacao, 5);
  Serial.print(F("MEDIA_PONTOS_G:")); Serial.println(f.soma / n, 4);
  Serial.print(F("MEDIA_3_PRIMEIROS_G:")); Serial.println(f.somaPrimeiros3 / 3.0f, 4);
  Serial.print(F("MEDIA_3_ULTIMOS_G:")); Serial.println(f.somaUltimos3 / 3.0f, 4);
  Serial.print(F("AMPLITUDE_ENTRE_PONTOS_G:")); Serial.println(f.maximo - f.minimo, 4);
  Serial.print(F("RUIDO_MEDIO_DP_DENTRO_PONTO_G:")); Serial.println(f.somaDP / n, 4);
  Serial.print(F("MAIOR_DP_DENTRO_PONTO_G:")); Serial.println(f.maiorDP, 4);
  Serial.print(F("MAIOR_AMPLITUDE_DENTRO_PONTO_G:")); Serial.println(f.maiorAmplitude, 4);
  Serial.print(F("PONTOS_AMPLITUDE_ACIMA_0_30G:")); Serial.println(f.alertasAmplitude);
}

bool executarFase(const __FlashStringHelper *nome, Fase &f, byte pontos,
                  byte duracaoMin, bool retornoZero = false) {
  zerarFase(f);
  Serial.println(F("========================================"));
  Serial.print(F("FASE:")); Serial.println(nome);
  Serial.println(F("ACOMODACAO:4S"));
  if (!aguardarMs(REPOUSO_MS)) return false;
  const unsigned long inicio = millis();

  if (retornoZero) {
    consecutivosRecuperacao = 0;
    primeiroTempoRecuperacaoS = -1;
  }

  for (byte i = 0; i < pontos; ++i) {
    const unsigned long marco = (unsigned long)i * INTERVALO_MS;
    while ((unsigned long)(millis() - inicio) < marco) {
      manterAtuadoresParados();
      if (lerComando() == 'S') {
        Serial.println(F("TESTE:CANCELADO"));
        return false;
      }
      delay(20);
    }
    const unsigned long inicioLeitura = millis();
    Medicao m;
    if (!medir(m)) return false;
    const unsigned long tempoS = (unsigned long)(inicioLeitura - inicio) / 1000UL;
    const float tempoMin = (float)(inicioLeitura - inicio) / 60000.0f;
    acumular(f, m, tempoMin, i, pontos);

    Serial.print(F("PONTO:")); Serial.print(nome);
    Serial.print(F(":")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(pontos);
    Serial.print(F(":TEMPO_S:")); Serial.print(tempoS);
    Serial.print(F(":PESO_MEDIO_G:")); Serial.print(m.peso, 4);
    Serial.print(F(":RUIDO_DP_G:")); Serial.print(m.dp, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(m.amplitude, 4);
    Serial.print(F(":ADC_MEDIO:")); Serial.print(m.mediaADC, 1);

    if (retornoZero) {
      const float residuo = m.peso - faseA.ultimo;
      Serial.print(F(":RESIDUO_VS_A_FINAL_G:")); Serial.print(residuo, 4);
      // Tres pontos consecutivos no intervalo: criterio operacional, nao
      // prova de retorno fisico completo nem substitui calibracao.
      if (fabsf(residuo) <= LIMIAR_RECUPERACAO_G) {
        if (consecutivosRecuperacao < 3) consecutivosRecuperacao++;
        if (consecutivosRecuperacao == 3 && primeiroTempoRecuperacaoS < 0) {
          primeiroTempoRecuperacaoS = (long)tempoS - 2L * (INTERVALO_MS / 1000UL);
        }
      } else {
        consecutivosRecuperacao = 0;
      }
    }
    if (m.amplitude > AMPLITUDE_AVISO_G) Serial.print(F(":AVISO:OSCILACAO"));
    Serial.println();
  }
  imprimirResumo(nome, f, duracaoMin);
  return true;
}

void relatorioFinal() {
  Serial.println(F("========================================"));
  Serial.println(F("RELATORIO_FINAL_RETORNO_ZERO_HX711_V10"));
  Serial.println(F("TARA_UNICA:SIM; PINOS:A1/A0; FATOR:-1129.6755"));
  Serial.print(F("DERIVA_VAZIO_5MIN_G:")); Serial.println(faseA.ultimo - faseA.primeiro, 4);
  Serial.print(F("DERIVA_CARGA_5MIN_G:")); Serial.println(faseB.ultimo - faseB.primeiro, 4);
  Serial.print(F("DERIVA_RETORNO_10MIN_G:")); Serial.println(faseC.ultimo - faseC.primeiro, 4);
  Serial.print(F("ZERO_REF_A_FINAL_G:")); Serial.println(faseA.ultimo, 4);
  Serial.print(F("ZERO_C_INICIAL_G:")); Serial.println(faseC.primeiro, 4);
  Serial.print(F("ZERO_C_FINAL_G:")); Serial.println(faseC.ultimo, 4);
  Serial.print(F("RESIDUO_C_INICIAL_MENOS_A_FINAL_G:"));
  Serial.println(faseC.primeiro - faseA.ultimo, 4);
  Serial.print(F("RESIDUO_C_FINAL_MENOS_A_FINAL_G:"));
  Serial.println(faseC.ultimo - faseA.ultimo, 4);
  Serial.print(F("RESIDUO_C_ULTIMOS3_MENOS_A_ULTIMOS3_G:"));
  Serial.println(faseC.somaUltimos3 / 3.0f - faseA.somaUltimos3 / 3.0f, 4);
  Serial.print(F("RUIDO_MEDIO_VAZIO_DP_G:")); Serial.println(faseA.somaDP / faseA.qtd, 4);
  Serial.print(F("RUIDO_MEDIO_CARGA_DP_G:")); Serial.println(faseB.somaDP / faseB.qtd, 4);
  Serial.print(F("RUIDO_MEDIO_RETORNO_DP_G:")); Serial.println(faseC.somaDP / faseC.qtd, 4);
  Serial.println(F("CRITERIO_RECUPERACAO:3_PONTOS_CONSECUTIVOS_ABS_RESIDUO_ATE_0_05G"));
  if (primeiroTempoRecuperacaoS >= 0) {
    Serial.print(F("PRIMEIRA_RECUPERACAO_APROX_S:"));
    Serial.println(primeiroTempoRecuperacaoS);
  } else {
    Serial.println(F("PRIMEIRA_RECUPERACAO_APROX_S:NAO_OBSERVADA"));
  }
  Serial.println(F("NOTA: recuperacao e avaliada contra zero de A_FINAL, nao ajustado por deriva temporal."));
  Serial.println(F("NOTA: este ensaio nao identifica sozinho a causa fisica da deriva."));
  Serial.println(F("SEM_AJUSTE_AUTOMATICO; SEM_EEPROM; SEM_AQUECIMENTO."));
}

void finalizar(const __FlashStringHelper *resultado) {
  manterAtuadoresParados();
  Serial.print(F("TESTE:FINALIZADO:")); Serial.println(resultado);
}

void setup() {
  configurarSaidas();
  Serial.begin(9600);
  balanca.begin(PINO_DT, PINO_SCK);
  Serial.println(F("========================================"));
  Serial.println(F("TESTE:RETORNO_ZERO_HX711_V10_5_5_10"));
  Serial.println(F("HX711 DT=A1 SCK=A0; fator -1129.6755 provisiorio."));
  Serial.println(F("FASES A:VAZIO 5min | B:CARGA 5min | C:RETORNO_ZERO 10min."));
  Serial.println(F("24 conversoes por ponto; um ponto a cada 30s."));
  Serial.println(F("Uma tara somente na fase A; SEM compensacao automatica."));
  Serial.println(F("Sem movimento; sem Servo.h; D6/D11 LOW."));
  Serial.println(F("IMPORTANTE: LOW pode energizar rele ativo-baixo."));
  Serial.println(F("GARANTA AQUECEDOR SEM POTENCIA E MOVIMENTO INATIVO."));
  Serial.println(F("Aqueça/estabilize eletronicamente a balanca por 30min antes de C."));

  if (!confirmar(F("Prato metalico VAZIO, balanca FIXA em X e aquecedor sem potencia?"))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!descartarConversoes()) { finalizar(F("ERRO_DESCARTE")); return; }
  if (!realizarTaraUnica()) { finalizar(F("ERRO_TARA")); return; }
  if (!executarFase(F("A_VAZIO"), faseA, PONTOS_5_MIN, 5)) {
    finalizar(F("ERRO_A")); return;
  }
  if (!confirmar(F("Coloque a MESMA massa de aprox. 33,4g sem mover o prato. Aguarde confirmacao."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!executarFase(F("B_CARGA"), faseB, PONTOS_5_MIN, 5)) {
    finalizar(F("ERRO_B")); return;
  }
  if (!confirmar(F("Retire completamente os 33,4g, sem mover o prato. Vamos observar 10min."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!executarFase(F("C_RETORNO_ZERO"), faseC, PONTOS_10_MIN, 10, true)) {
    finalizar(F("ERRO_C")); return;
  }
  relatorioFinal();
  finalizar(F("SUCESSO"));
}

void loop() {
  manterAtuadoresParados();
  delay(100);
}
