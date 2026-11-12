/*
  TCC - Analise automatizada de graos - V10
  VALIDACAO EM GRAMAS DO HX711 (3 ciclos, confirmacao serial)
  Arduino Uno | HX711 DT=A1, SCK=A0 | Celula 1 kg
  Biblioteca: HX711 (Bogde)

  O fator e PROVISORIO, obtido com 250 g nominais e embalagem lacrada.
  Com o prato instalado: confirmar -> tara -> ciclos A (vazio),
  B (com manteiga), C (vazio) -> resumo ao final.
  O peso do prato CONTINUA sobre a celula apos a tara.
  Nenhuma gravacao em EEPROM.
*/
#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>

const byte PIN_DT = A1;
const byte PIN_SCK = A0;
const float FATOR_CONTAGENS_POR_G = -1129.5576f;
const byte CICLOS = 3;
const byte MEDICOES = 10;
const byte LEITURAS_POR_MEDICAO = 10;
const float LIMIAR_AMPLITUDE_G = 0.50f;  // Alerta; nao e tolerancia certificada.

HX711 hx;

struct Estatistica {
  double media;
  double m2;
  float minimo;
  float maximo;
  byte n;
};

void falhar(const __FlashStringHelper *mensagem) {
  Serial.println();
  Serial.print(F("ERRO: "));
  Serial.println(mensagem);
  Serial.println(F("Teste interrompido. Verifique e reinicie."));
  while (true) delay(1000);
}

void confirmar(const __FlashStringHelper *mensagem) {
  while (Serial.available() > 0) Serial.read();
  Serial.println();
  Serial.println(mensagem);
  Serial.println(F("Digite C e pressione ENVIAR quando estiver pronto."));
  while (true) {
    if (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == 'C' || c == 'c') {
        delay(30);
        while (Serial.available() > 0) Serial.read();
        Serial.println(F("Confirmado. NAO toque no prato."));
        return;
      }
    }
    delay(20);
  }
}

void repousar() {
  Serial.println(F("Repouso de 30 segundos antes das leituras..."));
  for (int s = 30; s > 0; --s) {
    if (s % 5 == 0 || s <= 3) {
      Serial.print(F("Restam "));
      Serial.print(s);
      Serial.println(F(" s"));
    }
    delay(1000);
  }
}

bool leituraEmGramas(float &gramas) {
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_POR_MEDICAO; ++i) {
    if (!hx.wait_ready_timeout(4000)) return false;
    soma += (int64_t)hx.read();
  }
  double mediaBruta = (double)soma / LEITURAS_POR_MEDICAO;
  gramas = (float)((mediaBruta - (double)hx.get_offset()) /
                   (double)FATOR_CONTAGENS_POR_G);
  return true;
}

void inicializar(Estatistica &e) {
  e.media = 0.0;
  e.m2 = 0.0;
  e.minimo = 0.0f;
  e.maximo = 0.0f;
  e.n = 0;
}

void incluir(Estatistica &e, float valor) {
  if (e.n == 0) {
    e.minimo = valor;
    e.maximo = valor;
  }
  if (valor < e.minimo) e.minimo = valor;
  if (valor > e.maximo) e.maximo = valor;
  ++e.n;
  double d = (double)valor - e.media;
  e.media += d / e.n;
  e.m2 += d * ((double)valor - e.media);
}

double desvioPadrao(const Estatistica &e) {
  return e.n > 1 ? sqrt(e.m2 / (e.n - 1)) : 0.0;
}

Estatistica medir(byte ciclo, char etapa,
                 const __FlashStringHelper *instrucao) {
  confirmar(instrucao);
  repousar();
  Estatistica e;
  inicializar(e);
  for (byte i = 0; i < MEDICOES; ++i) {
    float peso;
    if (!leituraEmGramas(peso)) falhar(F("Timeout de leitura HX711"));
    incluir(e, peso);
    Serial.print(F("CICLO "));
    Serial.print(ciclo);
    Serial.print(F(" | ETAPA "));
    Serial.print(etapa);
    Serial.print(F(" | MEDICAO "));
    Serial.print(i + 1);
    Serial.print(F(" | PESO: "));
    Serial.print(peso, 3);
    Serial.println(F(" g"));
  }
  Serial.print(F("RESUMO "));
  Serial.print(etapa);
  Serial.print(F(" | media=")); Serial.print(e.media, 3);
  Serial.print(F(" g | amplitude="));
  Serial.print(e.maximo - e.minimo, 3);
  Serial.print(F(" g | desvio_padrao="));
  Serial.print(desvioPadrao(e), 3);
  Serial.println(F(" g"));

  if ((e.maximo - e.minimo) > LIMIAR_AMPLITUDE_G)
    Serial.println(F("ATENCAO: amplitude acima de 0,50 g nesta etapa."));
  return e;
}

void setup() {
  Serial.begin(9600);
  hx.begin(PIN_DT, PIN_SCK);
  delay(1000);
  Serial.println(F("=== CALIBRACAO HX711 V10 - PRECISAO ==="));
  Serial.println(F("Prato metalico instalado, celula de 1 kg."));
  Serial.print(F("Fator provisorio (contagens/g): "));
  Serial.println(FATOR_CONTAGENS_POR_G, 4);
  Serial.println(F("Referencia: 250 g NOMINAIS + massa da embalagem."));
  Serial.println(F("Nao gravamos fator ou tara na EEPROM."));
  if (!hx.wait_ready_timeout(5000)) falhar(F("HX711 nao respondeu"));

  confirmar(F("TARA INICIAL: deixe SOMENTE o prato metalico vazio."));
  repousar();
  if (!hx.wait_ready_timeout(4000)) falhar(F("HX711 nao respondeu antes da tara"));
  hx.set_scale(FATOR_CONTAGENS_POR_G);
  hx.tare(25);
  Serial.println(F("Tara realizada com o prato instalado."));

  double somaPesos = 0.0;
  double minimoPeso = 0.0;
  double maximoPeso = 0.0;
  double maiorRetorno = 0.0;

  for (byte ciclo = 1; ciclo <= CICLOS; ++ciclo) {
    Serial.println();
    Serial.println(F("======================================"));
    Serial.print(F("CICLO "));
    Serial.print(ciclo);
    Serial.print('/');
    Serial.println(CICLOS);
    Estatistica a = medir(ciclo, 'A',
      F("ETAPA A: prato metalico vazio, SEM manteiga."));
    Estatistica b = medir(ciclo, 'B',
      F("ETAPA B: coloque a manteiga lacrada no centro do prato."));
    Estatistica c = medir(ciclo, 'C',
      F("ETAPA C: retire a manteiga; prato continua instalado."));

    double linhaDeBase = (a.media + c.media) / 2.0;
    double pesoLiquido = b.media - linhaDeBase;
    double retorno = c.media - a.media;
    Serial.println(F("--- RESULTADO DO CICLO ---"));
    Serial.print(F("A vazio: ")); Serial.print(a.media, 3); Serial.println(F(" g"));
    Serial.print(F("B carregado: ")); Serial.print(b.media, 3); Serial.println(F(" g"));
    Serial.print(F("C vazio: ")); Serial.print(c.media, 3); Serial.println(F(" g"));
    Serial.print(F("Massa adicionada (corrigida pelo zero): "));
    Serial.print(pesoLiquido, 3); Serial.println(F(" g"));
    Serial.print(F("Desvio de retorno C-A: "));
    Serial.print(retorno, 3); Serial.println(F(" g"));
    Serial.println(F("Nota: correcao linear de zero e apenas diagnostica."));

    somaPesos += pesoLiquido;
    if (ciclo == 1 || pesoLiquido < minimoPeso) minimoPeso = pesoLiquido;
    if (ciclo == 1 || pesoLiquido > maximoPeso) maximoPeso = pesoLiquido;
    if (fabs(retorno) > maiorRetorno) maiorRetorno = fabs(retorno);
  }

  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F("RELATORIO FINAL - 3 CICLOS CONCLUIDOS"));
  Serial.println(F("======================================"));
  Serial.print(F("Massa media indicada (g): "));
  Serial.println(somaPesos / CICLOS, 3);
  Serial.print(F("Amplitude entre as 3 pesagens (g): "));
  Serial.println(maximoPeso - minimoPeso, 3);
  Serial.print(F("Maior desvio de retorno (g): "));
  Serial.println(maiorRetorno, 3);
  Serial.print(F("Fator usado, contagens/g: "));
  Serial.println(FATOR_CONTAGENS_POR_G, 4);
  Serial.println(F("250 g sao NOMINAIS, NAO massa aferida do conjunto."));
  Serial.println(F("Nao declarar exatidao absoluta sem referencia aferida."));
  Serial.println(F("FIM. Reinicie o Arduino para repetir."));
}

void loop() {}
