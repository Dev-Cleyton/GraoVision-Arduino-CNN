/*
 * TCC - TESTE COMPARATIVO DE PESAGEM NAS ALTURAS X E Y - V10
 * Arduino UNO; HX711 DT=A1, SCK=A0; stepper DIR=D2, STEP=D3.
 * Servo D9 SEM PWM em todo o diagnostico (bandeja deve ter apoio).
 * Rele D6 e aquecimento D11 mantidos DESLIGADOS.
 *
 * SEQUENCIA POR CICLO (3 ciclos, 1 tara apenas):
 * X vazio -> X com massa -> SUBIR -> Y com a MESMA massa
 * -> Y vazio -> DESCER -> X vazio (retorno).
 *
 * AVISO: 14000 passos correspondem ao firmware V10 enviado, mas NAO
 * existe leitura de fim de curso nem confirmacao de movimento.
 * Somente autorize movimentos se o conjunto estiver em X, com trajeto
 * livre, dentro do curso seguro. Nao opere sem supervisao.
 *
 * A massa precisa permanecer igual entre X e Y, sem aquecimento.
 * O fator contagens/g e PROVISORIO (250 g nominais da manteiga).
 * Os deslocamentos de zero e as massas relativas sao comparaveis.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>

const byte PINO_DT = A1;
const byte PINO_SCK = A0;
const byte PINO_DIR = 2;
const byte PINO_STEP = 3;
const byte PINO_RELE = 6;
const byte PINO_SERVO = 9;
const byte PINO_HEATER = 11;

const float FATOR_CONTAGENS_POR_G = -1129.6755f;
const long PASSOS_ENTRE_X_Y = 14000L; // valor do firmware V10
const unsigned int PULSO_US = 300;
const unsigned long REPouso_MS = 4000UL;
const unsigned long HX_TIMEOUT_MS = 2500UL;
const byte DESCARTES = 4;
const byte AMOSTRAS = 12;
const byte MAX_JANELAS = 8;
const float LIMITE_AMPLITUDE_G = 0.30f;
const float LIMITE_MEDIA_G = 0.12f;
const byte TOTAL_CICLOS = 3;

HX711 balanca;

struct Ciclo {
  float xVazio;
  float xCarga;
  float yCarga;
  float yVazio;
  float xRetorno;
  float offsetY;
  float massaX;
  float massaY;
  float diferencaMassa;
  float retornoX;
};

Ciclo resultados[TOTAL_CICLOS];

void desligarSaidas() {
  digitalWrite(PINO_RELE, LOW);
  digitalWrite(PINO_HEATER, LOW);
  // SEM PWM de servo. Nao alimentar diretamente o servo pelo Arduino.
  digitalWrite(PINO_SERVO, LOW);
}

// Apenas comandos digitados pelo operador. 'SAIR' cancela imediatamente
// enquanto o sketch estiver esperando; movimento ja iniciado e bloqueante.
bool esperarComando(const char *esperado) {
  Serial.print(F("Digite "));
  Serial.print(esperado);
  Serial.println(F(" e pressione ENVIAR. (SAIR cancela)"));
  while (true) {
    desligarSaidas();
    if (!Serial.available()) { delay(30); continue; }
    String s = Serial.readStringUntil('\n');
    s.trim();
    s.toUpperCase();
    if (!s.length()) continue;
    if (s == F("SAIR")) {
      Serial.println(F("TESTE:CANCELADO; NAO MOVER AUTOMATICAMENTE."));
      return false;
    }
    if (s.equalsIgnoreCase(esperado)) {
      Serial.println(F("CONFIRMADO"));
      return true;
    }
    Serial.println(F("COMANDO INVALIDO. Confirme a etapa correta."));
  }
}

bool sensorPronto() {
  if (!balanca.wait_ready_timeout(HX_TIMEOUT_MS)) {
    Serial.println(F("HX711:ERRO:TIMEOUT"));
    return false;
  }
  return true;
}

bool executarTaraInicial() {
  Serial.println(F("TARA: aguarde 4 s com prato vazio em X; nao toque."));
  delay(REPouso_MS);
  int64_t soma = 0;
  for (byte i=0; i<20; i++) {
    if (!sensorPronto()) return false;
    soma += balanca.read();
  }
  balanca.set_offset((long)(soma / 20));
  Serial.println(F("TARA:OK:UNICA_PARA_TODO_TESTE"));
  return true;
}

bool medirEstavel(float &resultado) {
  desligarSaidas();
  Serial.println(F("HX711:AGUARDANDO_REPOUSO_4S"));
  delay(REPouso_MS);
  for (byte i=0; i<DESCARTES; i++) {
    if (!sensorPronto()) return false;
    balanca.read();
  }
  bool anteriorEstavel = false;
  float mediaAnterior = 0.0f;
  for (byte j=1; j<=MAX_JANELAS; j++) {
    float soma = 0.0f, minimo = 0.0f, maximo = 0.0f;
    for (byte i=0; i<AMOSTRAS; i++) {
      if (!sensorPronto()) return false;
      float valor = balanca.get_units(1);
      if (isnan(valor) || isinf(valor)) {
        Serial.println(F("HX711:ERRO:LEITURA_INVALIDA"));
        return false;
      }
      if (i == 0 || valor < minimo) minimo = valor;
      if (i == 0 || valor > maximo) maximo = valor;
      soma += valor;
      delay(100);
    }
    float media = soma / AMOSTRAS;
    float amplitude = maximo - minimo;
    Serial.print(F("HX711:JANELA:")); Serial.print(j);
    Serial.print(F(":MEDIA:")); Serial.print(media, 3);
    Serial.print(F(":AMPLITUDE:")); Serial.println(amplitude, 3);
    if (amplitude <= LIMITE_AMPLITUDE_G) {
      if (anteriorEstavel && fabs(media - mediaAnterior) <= LIMITE_MEDIA_G) {
        resultado = (media + mediaAnterior) * 0.5f;
        Serial.println(F("HX711:ESTAVEL"));
        return true;
      }
      mediaAnterior = media;
      anteriorEstavel = true;
    } else anteriorEstavel = false;
    Serial.print(F("HX711:INSTAVEL:")); Serial.println(amplitude, 3);
  }
  Serial.println(F("HX711:ERRO:SEM_ESTABILIDADE"));
  return false;
}

bool registrar(const __FlashStringHelper *etapa, float &valor) {
  Serial.print(F("ETAPA:")); Serial.println(etapa);
  while (true) {
    if (medirEstavel(valor)) {
      Serial.print(F("RESULTADO:")); Serial.print(etapa);
      Serial.print(F(":")); Serial.println(valor, 4);
      return true;
    }
    Serial.println(F("MEDICAO FALHOU. Sem alterar tara ou fator."));
    if (!esperarComando("R")) return false;
  }
}

// Comando explicito do operador antes de cada movimento. Esta rotina nao
// possui sensor de fim de curso: interrompa a alimentacao em emergencia.
void moverVertical(bool subir) {
  desligarSaidas();
  digitalWrite(PINO_DIR, subir ? HIGH : LOW); // mesma direcao da V10
  delay(10);
  for (long i=0; i<PASSOS_ENTRE_X_Y; i++) {
    digitalWrite(PINO_STEP, HIGH);
    delayMicroseconds(PULSO_US);
    digitalWrite(PINO_STEP, LOW);
    delayMicroseconds(PULSO_US);
  }
  Serial.print(F("MOVIMENTO:PASSOS_COMANDADOS:"));
  Serial.println(PASSOS_ENTRE_X_Y);
  Serial.println(F("ATENCAO: nao ha sensor confirmando a posicao final."));
}

void imprimirResumoCiclo(byte n) {
  Ciclo &c = resultados[n];
  c.offsetY = c.yVazio - c.xVazio;
  c.massaX = c.xCarga - c.xVazio;
  c.massaY = c.yCarga - c.yVazio;
  c.diferencaMassa = c.massaY - c.massaX;
  c.retornoX = c.xRetorno - c.xVazio;
  Serial.println(F("----- RESUMO DO CICLO -----"));
  Serial.print(F("CICLO:")); Serial.println(n+1);
  Serial.print(F("ZERO_X:")); Serial.println(c.xVazio, 4);
  Serial.print(F("ZERO_Y:")); Serial.println(c.yVazio, 4);
  Serial.print(F("OFFSET_Y_MENOS_X:")); Serial.println(c.offsetY, 4);
  Serial.print(F("MASSA_X_CORRIGIDA:")); Serial.println(c.massaX, 4);
  Serial.print(F("MASSA_Y_CORRIGIDA:")); Serial.println(c.massaY, 4);
  Serial.print(F("DIFERENCA_MASSA_Y_MENOS_X:")); Serial.println(c.diferencaMassa, 4);
  Serial.print(F("ZERO_X_APOS_RETORNO:")); Serial.println(c.xRetorno, 4);
  Serial.print(F("DERIVA_X_RETORNO:")); Serial.println(c.retornoX, 4);
}

void relatorioFinal() {
  float somaOffset=0, somaDiferenca=0, somaRetorno=0;
  float minOffset=0,maxOffset=0,minDif=0,maxDif=0;
  for (byte i=0; i<TOTAL_CICLOS; i++) {
    Ciclo &c=resultados[i];
    somaOffset += c.offsetY;
    somaDiferenca += c.diferencaMassa;
    somaRetorno += c.retornoX;
    if (i == 0 || c.offsetY < minOffset) minOffset=c.offsetY;
    if (i == 0 || c.offsetY > maxOffset) maxOffset=c.offsetY;
    if (i == 0 || c.diferencaMassa < minDif) minDif=c.diferencaMassa;
    if (i == 0 || c.diferencaMassa > maxDif) maxDif=c.diferencaMassa;
  }
  Serial.println(F("====================================="));
  Serial.println(F("RELATORIO_FINAL_ALTURAS_V10"));
  Serial.print(F("OFFSET_Y_X_MEDIO_G:")); Serial.println(somaOffset/TOTAL_CICLOS, 4);
  Serial.print(F("AMPLITUDE_OFFSET_G:")); Serial.println(maxOffset-minOffset, 4);
  Serial.print(F("ERRO_MASSA_Y_X_MEDIO_G:")); Serial.println(somaDiferenca/TOTAL_CICLOS, 4);
  Serial.print(F("AMPLITUDE_ERRO_MASSA_G:")); Serial.println(maxDif-minDif, 4);
  Serial.print(F("RETORNO_X_MEDIO_G:")); Serial.println(somaRetorno/TOTAL_CICLOS, 4);
  Serial.println(F("SEM_COMPENSACAO_AUTOMATICA; SEM_AQUECIMENTO"));
  Serial.println(F("A massa real e o fator ainda precisam de referencia aferida."));
  Serial.println(F("FIM:TESTE_ALTURAS"));
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(350);
  pinMode(PINO_DIR, OUTPUT);
  pinMode(PINO_STEP, OUTPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_HEATER, OUTPUT);
  pinMode(PINO_SERVO, OUTPUT);
  digitalWrite(PINO_STEP, LOW);
  desligarSaidas();
  balanca.begin(PINO_DT, PINO_SCK);
  balanca.set_scale(FATOR_CONTAGENS_POR_G);
  Serial.println(F("====================================="));
  Serial.println(F("VALIDACAO DAS ALTURAS X/Y - HX711 V10"));
  Serial.println(F("3 ciclos; uma tara inicial X; SEM aquecimento."));
  Serial.println(F("SEM servo PWM; movimentos somente autorizados."));
  Serial.println(F("Motor: 14000 passos por percurso X/Y."));
  Serial.println(F("SEM fim de curso no codigo: confira curso livre."));
  Serial.println(F("NENHUM OFFSET sera gravado ou aplicado."));
  Serial.println(F("IMPORTANTE: comecar fisicamente EM X (embaixo)."));
  if (!sensorPronto()) {
    Serial.println(F("ERRO: HX711 nao respondeu. Reinicie apos corrigir."));
    return;
  }
  Serial.println(F("PRATO VAZIO em X; confira cabos livres e percurso seguro."));
  if (!esperarComando("C")) return;
  if (!executarTaraInicial()) return;

  for (byte n=0; n<TOTAL_CICLOS; n++) {
    Serial.println(F("====================================="));
    Serial.print(F("CICLO:")); Serial.print(n+1); Serial.print('/'); Serial.println(TOTAL_CICLOS);
    Serial.println(F("X INFERIOR: prato vazio, sem tocar na balanca."));
    if (!esperarComando("C")) return;
    if (!registrar(F("X_VAZIO"), resultados[n].xVazio)) return;

    Serial.println(F("X INFERIOR: coloque a MESMA massa/referencia no centro."));
    Serial.println(F("Mantenha a massa sobre o prato durante a subida."));
    if (!esperarComando("C")) return;
    if (!registrar(F("X_CARGA"), resultados[n].xCarga)) return;

    Serial.println(F("AUTORIZAR SUBIDA X->Y, com carga e percurso livre?"));
    Serial.println(F("Verifique limite mecanico e estabilidade da amostra."));
    if (!esperarComando("SUBIR")) return;
    moverVertical(true);
    Serial.println(F("CONFIRME: conjunto realmente chegou a Y e esta parado."));
    if (!esperarComando("C")) return;
    if (!registrar(F("Y_CARGA"), resultados[n].yCarga)) return;

    Serial.println(F("Y SUPERIOR: retire SOMENTE a massa; mantenha o prato."));
    if (!esperarComando("C")) return;
    if (!registrar(F("Y_VAZIO"), resultados[n].yVazio)) return;

    Serial.println(F("AUTORIZAR DESCIDA Y->X com PRATO VAZIO?"));
    if (!esperarComando("DESCER")) return;
    moverVertical(false);
    Serial.println(F("CONFIRME: conjunto realmente voltou a X e esta parado."));
    if (!esperarComando("C")) return;
    if (!registrar(F("X_RETORNO"), resultados[n].xRetorno)) return;
    imprimirResumoCiclo(n);
  }
  relatorioFinal();
}

void loop() {
  desligarSaidas();
  delay(250);
}
