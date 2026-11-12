/*
  TCC - Analise automatizada de graos - V10
  DIAGNOSTICO DO HX711 COM 3 CICLOS (A/B/C)
  Arduino Uno | HX711 DT=A1 SCK=A0 | Celula 1 kg | prato metalico
  Biblioteca: HX711 (Bogde)

  Cada etapa: usuario digita C -> 30 s de repouso -> 10 medicoes,
  cada uma como media de 10 conversoes. Sem tara nem escala.
  Ao final: media, amplitude, desvio-padrao, repetibilidade e fator
  PROVISORIO com massa nominal de 250 g (embalagem nao conhecida).
  NAO grava EEPROM e NAO modifica o projeto principal.
*/
#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>

const byte PIN_DT = A1;
const byte PIN_SCK = A0;
const byte CICLOS = 3;
const byte MEDICOES = 10;
const byte LEITURAS_POR_MEDICAO = 10;
const float MASSA_NOMINAL_G = 250.0f;

HX711 hx;

struct Estatistica {
  double media;
  double m2;
  long minimo;
  long maximo;
  byte n;
};

void falhar(const __FlashStringHelper *mensagem) {
  Serial.println();
  Serial.print(F("ERRO: "));
  Serial.println(mensagem);
  Serial.println(F("Teste interrompido. Confira conexoes e reinicie."));
  while (true) delay(1000);
}

void limparEntrada() {
  while (Serial.available() > 0) Serial.read();
}

void confirmar(const __FlashStringHelper *mensagem) {
  limparEntrada();
  Serial.println();
  Serial.println(mensagem);
  Serial.println(F("Quando terminar, digite C e pressione ENVIAR."));
  while (true) {
    if (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == 'C' || c == 'c') {
        delay(30);  // Permite receber final de linha da IDE.
        limparEntrada();
        Serial.println(F("Confirmado. NAO toque na balanca."));
        return;
      }
    }
    delay(20);
  }
}

void repousar() {
  Serial.println(F("Estabilizacao: 30 segundos."));
  for (int s = 30; s > 0; --s) {
    if (s % 5 == 0 || s <= 3) {
      Serial.print(F("Restam "));
      Serial.print(s);
      Serial.println(F(" s"));
    }
    delay(1000);
  }
}

bool lerBrutoMedio(long &saida) {
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_POR_MEDICAO; ++i) {
    if (!hx.wait_ready_timeout(4000)) return false;
    soma += (int64_t)hx.read();
  }
  saida = (long)(soma / LEITURAS_POR_MEDICAO);
  return true;
}

void limparEstat(Estatistica &e) {
  e.media = 0.0;
  e.m2 = 0.0;
  e.minimo = 0;
  e.maximo = 0;
  e.n = 0;
}

void adicionar(Estatistica &e, long valor) {
  if (e.n == 0) {
    e.minimo = valor;
    e.maximo = valor;
  }
  if (valor < e.minimo) e.minimo = valor;
  if (valor > e.maximo) e.maximo = valor;
  e.n++;
  double dif = (double)valor - e.media;
  e.media += dif / e.n;
  double dif2 = (double)valor - e.media;
  e.m2 += dif * dif2;
}

double desvioPadrao(const Estatistica &e) {
  return (e.n > 1) ? sqrt(e.m2 / (e.n - 1)) : 0.0;
}

Estatistica medirEtapa(byte ciclo, char etapa,
                      const __FlashStringHelper *instrucao) {
  confirmar(instrucao);
  repousar();
  Estatistica e;
  limparEstat(e);
  for (byte i = 0; i < MEDICOES; ++i) {
    long valor;
    if (!lerBrutoMedio(valor)) falhar(F("Timeout esperando HX711"));
    adicionar(e, valor);
    Serial.print(F("CICLO "));
    Serial.print(ciclo);
    Serial.print(F(" | ETAPA "));
    Serial.print(etapa);
    Serial.print(F(" | MEDICAO "));
    Serial.print(i + 1);
    Serial.print('/');
    Serial.print(MEDICOES);
    Serial.print(F(" | ADC: "));
    Serial.println(valor);
  }
  Serial.print(F("RESUMO "));
  Serial.print(etapa);
  Serial.print(F(" | media="));
  Serial.print(e.media, 2);
  Serial.print(F(" | min="));
  Serial.print(e.minimo);
  Serial.print(F(" | max="));
  Serial.print(e.maximo);
  Serial.print(F(" | amplitude="));
  Serial.print(e.maximo - e.minimo);
  Serial.print(F(" | desvio_padrao="));
  Serial.println(desvioPadrao(e), 2);
  return e;
}

void setup() {
  Serial.begin(9600);
  hx.begin(PIN_DT, PIN_SCK);
  delay(1000);
  Serial.println(F("=== TESTE ADC HX711 V10 - PRECISAO ==="));
  Serial.println(F("Celula: 1 kg; prato metalico fixo."));
  Serial.println(F("3 ciclos A/B/C; 10 medicoes por etapa; 30 s de repouso."));
  Serial.println(F("A: prato vazio / B: prato + manteiga / C: prato vazio."));
  Serial.println(F("Importante: limite de 1 kg inclui o prato!"));
  Serial.println(F("Sem tara e sem calibracao; resultados em contagens."));
  if (!hx.wait_ready_timeout(5000)) falhar(F("HX711 nao respondeu"));

  double somaFatores = 0.0;
  double minFator = 0.0;
  double maxFator = 0.0;
  double maxRetorno = 0.0;
  for (byte ciclo = 1; ciclo <= CICLOS; ++ciclo) {
    Serial.println();
    Serial.println(F("======================================"));
    Serial.print(F("INICIO DO CICLO "));
    Serial.print(ciclo);
    Serial.print('/');
    Serial.println(CICLOS);

    Estatistica a = medirEtapa(ciclo, 'A',
      F("ETAPA A: deixe SOMENTE o prato metalico, vazio."));
    Estatistica b = medirEtapa(ciclo, 'B',
      F("ETAPA B: coloque a manteiga lacrada no centro do prato."));
    Estatistica c = medirEtapa(ciclo, 'C',
      F("ETAPA C: retire SOMENTE a manteiga; mantenha o prato."));

    // Compensa linearmente eventual deslocamento do zero entre A e C.
    double zeroInterpolado = (a.media + c.media) / 2.0;
    double delta = b.media - zeroInterpolado;
    double retorno = c.media - a.media;
    if (fabs(delta) < 1.0) falhar(F("Variacao de ADC insuficiente"));
    double fator = delta / MASSA_NOMINAL_G;

    Serial.println(F("--- RESULTADO DO CICLO ---"));
    Serial.print(F("Media A: ")); Serial.println(a.media, 2);
    Serial.print(F("Media B: ")); Serial.println(b.media, 2);
    Serial.print(F("Media C: ")); Serial.println(c.media, 2);
    Serial.print(F("Delta com zero interpolado: ")); Serial.println(delta, 2);
    Serial.print(F("Retorno (C-A), contagens: ")); Serial.println(retorno, 2);
    Serial.print(F("Fator estimado, contagens/g: ")); Serial.println(fator, 4);

    somaFatores += fator;
    if (ciclo == 1 || fator < minFator) minFator = fator;
    if (ciclo == 1 || fator > maxFator) maxFator = fator;
    if (fabs(retorno) > maxRetorno) maxRetorno = fabs(retorno);
  }

  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F("RELATORIO FINAL - 3 CICLOS CONCLUIDOS"));
  Serial.println(F("======================================"));
  Serial.print(F("Fator medio PROVISORIO (contagens/g): "));
  Serial.println(somaFatores / CICLOS, 4);
  Serial.print(F("Amplitude entre fatores (contagens/g): "));
  Serial.println(maxFator - minFator, 4);
  Serial.print(F("Maior desvio de retorno (contagens): "));
  Serial.println(maxRetorno, 2);
  Serial.println(F("Massa usada: 250 g NOMINAIS; embalagem soma peso."));
  Serial.println(F("SEM EEPROM. Validar massa real para exatidao absoluta."));
  Serial.println(F("TESTE FINALIZADO. Para repetir, reinicie o Arduino."));
}

void loop() {}
