/*
 * ==========================================================
 * TCC - SISTEMA DE ANALISE AUTOMATIZADA DE GRAOS
 * VERSAO 10 - DIAGNOSTICO E CALIBRACAO DO HX711
 * ==========================================================
 *
 * Microcontrolador: Arduino Uno
 * Sensor: HX711 + Celula de carga de 1 kg
 *
 * Ligacoes:
 *   HX711 DT  -> A1
 *   HX711 SCK -> A0
 *   HX711 VCC -> 5V
 *   HX711 GND -> GND
 *
 * TESTE:
 * A - Prato metalico vazio
 * B - Prato + massa de referencia
 * C - Prato vazio novamente
 *
 * Cada etapa:
 * - Confirmacao manual
 * - 30 segundos de estabilizacao
 * - 10 medicoes
 * - Cada medicao usa 10 leituras brutas
 *
 * O resumo aparece somente ao concluir as 3 etapas.
 * ==========================================================
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>

// ==========================================================
// CONFIGURACOES
// ==========================================================

#define HX711_DT  A1
#define HX711_SCK A0

const unsigned long TEMPO_ESTABILIZACAO = 30000UL;

const byte NUM_MEDICOES = 10;
const byte LEITURAS_POR_MEDICAO = 10;

// Valor nominal da manteiga.
// A embalagem acrescenta massa desconhecida.
const float MASSA_REFERENCIA = 267.0;

// ==========================================================
// OBJETOS E ESTRUTURAS
// ==========================================================

HX711 balanca;

struct ResultadoEtapa {
  float media;
  long minimo;
  long maximo;
  bool valido;
};

ResultadoEtapa etapaA;
ResultadoEtapa etapaB;
ResultadoEtapa etapaC;

// ==========================================================
// AGUARDAR CONFIRMACAO DO USUARIO
// ==========================================================

void aguardarConfirmacao() {

  // Remove comandos antigos que possam estar no buffer.
  while (Serial.available() > 0) {
    Serial.read();
  }

  Serial.println();
  Serial.println(F("Digite C e pressione ENVIAR"));
  Serial.println(F("quando a etapa estiver preparada."));
  Serial.println(F("Aguardando confirmacao..."));

  while (true) {

    if (Serial.available() > 0) {

      char comando = Serial.read();

      if (comando == 'C' || comando == 'c') {

        // Descarta eventuais caracteres restantes.
        while (Serial.available() > 0) {
          Serial.read();
        }

        Serial.println(F("CONFIRMACAO RECEBIDA."));
        return;
      }
    }

    delay(50);
  }
}

// ==========================================================
// AGUARDAR ESTABILIZACAO
// ==========================================================

void aguardarEstabilizacao() {

  Serial.println();
  Serial.println(F("Iniciando estabilizacao de 30 segundos."));
  Serial.println(F("NAO TOQUE NA BALANCA."));

  for (int segundos = 30; segundos > 0; segundos--) {

    if (segundos % 5 == 0 || segundos <= 3) {

      Serial.print(F("Tempo restante: "));
      Serial.print(segundos);
      Serial.println(F(" segundos"));
    }

    delay(1000);
  }

  Serial.println();
  Serial.println(F("ESTABILIZACAO CONCLUIDA."));
  Serial.println(F("Iniciando aquisicao das leituras..."));
}

// ==========================================================
// OBTER UMA MEDICAO PELA MEDIA DE 10 LEITURAS BRUTAS
// ==========================================================

bool obterMedicao(long &resultado) {

  int64_t soma = 0;

  for (byte i = 0; i < LEITURAS_POR_MEDICAO; i++) {

    if (!balanca.wait_ready_timeout(3000)) {

      Serial.println(F("ERRO: HX711 nao respondeu."));
      return false;
    }

    soma += balanca.read();
  }

  resultado = (long)(soma / LEITURAS_POR_MEDICAO);

  return true;
}

// ==========================================================
// EXECUTAR UMA ETAPA
// ==========================================================

ResultadoEtapa executarEtapa(char identificador) {

  ResultadoEtapa resultado;

  resultado.media = 0;
  resultado.minimo = 0;
  resultado.maximo = 0;
  resultado.valido = false;

  aguardarEstabilizacao();

  float somaMedicoes = 0;

  for (byte i = 0; i < NUM_MEDICOES; i++) {

    long leitura;

    if (!obterMedicao(leitura)) {
      return resultado;
    }

    if (i == 0) {
      resultado.minimo = leitura;
      resultado.maximo = leitura;
    }

    if (leitura < resultado.minimo) {
      resultado.minimo = leitura;
    }

    if (leitura > resultado.maximo) {
      resultado.maximo = leitura;
    }

    somaMedicoes += leitura;

    Serial.print(F("ETAPA "));
    Serial.print(identificador);

    Serial.print(F(" | Medicao "));
    Serial.print(i + 1);
    Serial.print(F("/"));
    Serial.print(NUM_MEDICOES);

    Serial.print(F(" | ADC BRUTO: "));
    Serial.println(leitura);
  }

  resultado.media = somaMedicoes / NUM_MEDICOES;
  resultado.valido = true;

  Serial.println();
  Serial.print(F("ETAPA "));
  Serial.print(identificador);
  Serial.println(F(" CONCLUIDA."));

  return resultado;
}

// ==========================================================
// MOSTRAR RESULTADOS
// ==========================================================

void mostrarResultadoEtapa(
  char identificador,
  ResultadoEtapa resultado
) {

  Serial.print(F("ETAPA "));
  Serial.println(identificador);

  Serial.print(F("Media ADC: "));
  Serial.println(resultado.media, 2);

  Serial.print(F("Minimo: "));
  Serial.println(resultado.minimo);

  Serial.print(F("Maximo: "));
  Serial.println(resultado.maximo);

  Serial.print(F("Amplitude: "));
  Serial.println(resultado.maximo - resultado.minimo);

  Serial.println();
}

// ==========================================================
// RELATORIO FINAL
// ==========================================================

void gerarRelatorioFinal() {

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" RELATORIO FINAL - HX711 V10"));
  Serial.println(F("=========================================="));

  mostrarResultadoEtapa('A', etapaA);
  mostrarResultadoEtapa('B', etapaB);
  mostrarResultadoEtapa('C', etapaC);

  float deltaCarga = etapaB.media - etapaA.media;

  float desvioZero = etapaC.media - etapaA.media;

  Serial.println(F("------------------------------------------"));
  Serial.println(F("ANALISE COMPARATIVA"));
  Serial.println(F("------------------------------------------"));

  Serial.print(F("Delta ADC (B - A): "));
  Serial.println(deltaCarga, 2);

  Serial.print(F("Desvio de retorno (C - A): "));
  Serial.println(desvioZero, 2);

  if (fabs(deltaCarga) > 1.0) {

    float erroRetorno =
      (fabs(desvioZero) / fabs(deltaCarga)) * 100.0;

    Serial.print(F("Desvio relativo de retorno: "));
    Serial.print(erroRetorno, 2);
    Serial.println(F(" %"));

    float fatorEstimado = deltaCarga / MASSA_REFERENCIA;

    Serial.println();
    Serial.println(F("CALIBRACAO APROXIMADA"));

    Serial.print(F("Massa nominal de referencia: "));
    Serial.print(MASSA_REFERENCIA, 2);
    Serial.println(F(" g"));

    Serial.print(F("Fator estimado (contagens/g): "));
    Serial.println(fatorEstimado, 4);

    Serial.println(F("ATENCAO: inclui erro da embalagem."));
    Serial.println(F("NAO utilizar como fator definitivo."));
  }

  else {

    Serial.println(F("ERRO: resposta insuficiente a carga."));
    Serial.println(F("Nao foi possivel estimar o fator."));
  }

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" TESTE FINALIZADO"));
  Serial.println(F("=========================================="));
}

// ==========================================================
// SETUP
// ==========================================================

void setup() {

  Serial.begin(9600);

  balanca.begin(HX711_DT, HX711_SCK);

  delay(2000);

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" TESTE AUTOMATIZADO HX711 - V10"));
  Serial.println(F("=========================================="));

  Serial.println(F("Celula de carga: 1 kg"));
  Serial.println(F("Prato: metalico"));
  Serial.println(F("Referencia nominal: 267 g"));

  Serial.println();
  Serial.println(F("Este teste possui 3 etapas:"));
  Serial.println(F("A - Prato vazio"));
  Serial.println(F("B - Prato com a manteiga"));
  Serial.println(F("C - Prato vazio novamente"));

  Serial.println();
  Serial.println(F("Cada etapa registra 10 medicoes"));
  Serial.println(F("apos 30 segundos de estabilizacao."));

  if (!balanca.wait_ready_timeout(5000)) {

    Serial.println(F("ERRO: HX711 nao detectado."));
    Serial.println(F("Verifique os fios DT e SCK."));

    while (true) {
      delay(1000);
    }
  }

  // ========================================================
  // ETAPA A
  // ========================================================

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" ETAPA A - PRATO VAZIO"));
  Serial.println(F("=========================================="));

  Serial.println(F("Deixe SOMENTE o prato metalico instalado."));
  Serial.println(F("Nao coloque a manteiga ainda."));

  aguardarConfirmacao();

  etapaA = executarEtapa('A');

  if (!etapaA.valido) {
    Serial.println(F("Falha na etapa A. Reinicie o teste."));
    return;
  }

  // ========================================================
  // ETAPA B
  // ========================================================

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" ETAPA B - COLOCAR A CARGA"));
  Serial.println(F("=========================================="));

  Serial.println(F("AGORA COLOQUE A MANTEIGA DE 267 g."));
  Serial.println(F("Centralize a embalagem no prato."));
  Serial.println(F("Nao encoste em outras partes da estrutura."));

  aguardarConfirmacao();

  etapaB = executarEtapa('B');

  if (!etapaB.valido) {
    Serial.println(F("Falha na etapa B. Reinicie o teste."));
    return;
  }

  // ========================================================
  // ETAPA C
  // ========================================================

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" ETAPA C - RETIRAR A CARGA"));
  Serial.println(F("=========================================="));

  Serial.println(F("AGORA RETIRE A MANTEIGA."));
  Serial.println(F("Mantenha o prato metalico instalado."));

  aguardarConfirmacao();

  etapaC = executarEtapa('C');

  if (!etapaC.valido) {
    Serial.println(F("Falha na etapa C. Reinicie o teste."));
    return;
  }

  // ========================================================
  // RESULTADO
  // ========================================================

  gerarRelatorioFinal();
}

// ==========================================================
// LOOP
// ==========================================================

void loop() {

  // O teste e executado uma unica vez.
  // Para repetir, reinicie o Arduino.

}
