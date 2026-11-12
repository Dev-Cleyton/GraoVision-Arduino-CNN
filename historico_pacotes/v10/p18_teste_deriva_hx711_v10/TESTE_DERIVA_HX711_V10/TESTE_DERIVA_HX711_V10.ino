/*
  TCC Arduino_Grao_CNN - V10
  TESTE:DERIVA_HX711_V10

  Objetivo: diagnosticar deriva de zero e deriva com carga, sem qualquer
  movimento dos atuadores e sem usar nova tara entre as etapas.

  FASE A: bandeja vazia em X, 10 minutos, uma medicao a cada 30 segundos.
  FASE B: mesma bandeja com massa fixa, 10 minutos, mesmas medicoes.
  FINAL: retirada da massa, verificar retorno ao zero (medicao adicional).

  IMPORTANTE:
  - Desconectar fisicamente a alimentacao do aquecedor.
  - Servo sem PWM (nao usa Servo.h nem chama attach()).
  - Motor sem pulsos STEP. Balança parada na altura X em todo o teste.
  - Uma unica tara inicial, com bandeja instalada e VAZIA.
  - Sem EEPROM, sem controle de movimento, sem Python/COM4 durante o teste.
  - D6/D11 mantidos LOW: verificar se seu rele e ativo em LOW; a
    desconexao fisica do aquecedor e obrigatoria de qualquer maneira.
  - O fator -1129.6755 eh PROVISORIO, usando 250 g nominais.
  - A capacidade de 1 kg INCLUI a bandeja e qualquer carga.

  Pinos: HX711 DT=A1, SCK=A0; STEP=D3, DIR=D2, rele=D6,
         heater=D11, servo sinal=D9.

  Resultado: resumos a cada 30 s em gramas; amplitudes e desvio-padrao
  de cada bloco de leituras; deriva total e inclinacao estimada em g/min.
*/

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

const byte PIN_DT = A1;
const byte PIN_SCK = A0;
const byte PIN_DIR = 2;
const byte PIN_STEP = 3;
const byte PIN_RELE = 6;
const byte PIN_SERVO = 9;
const byte PIN_HEATER = 11;

const float FATOR_CONTAGENS_POR_G = -1129.6755f;
const unsigned long DURACAO_MS = 600000UL; // 10 min
const unsigned long INTERVALO_MS = 30000UL; // 30 s
const unsigned long REPOUSO_MS = 4000UL;
const unsigned long TIMEOUT_HX_MS = 2500UL;
const byte LEITURAS_TARA = 20;
const byte LEITURAS_POR_PONTO = 12;
const float LIMITE_AMPLITUDE_G = 0.30f; // sinalizacao, nao descarta dado
const byte TOTAL_PONTOS = 21; // tempo 0, 30, ..., 600 s

HX711 balanca;
long offsetAdc = 0;

// Parser de comandos: linhas curtas, sem uso de String dinamica no UNO.
char comandoBuffer[20];
byte comandoPos = 0;

struct Ponto {
  float peso;
  float amplitude;
  float dp;
  float adc;
};

struct Resumo {
  bool valido;
  float primeiro;
  float ultimo;
  float media;
  float minimo;
  float maximo;
  float maiorAmplitudeInterna;
  float maiorDesvioPadraoInterno;
  float deriva;
  float inclinacaoGMin;
  byte pontosRuidoAlto;
  byte quantidade;
};

Resumo vazio;
Resumo carregado;

void saidasSeguras() {
  // Nao usamos a biblioteca Servo neste sketch: sem pulsos PWM.
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_SERVO, LOW);
}

void prepararSaidas() {
  // Define nivel antes de habilitar OUTPUT para evitar pulsos espurios.
  digitalWrite(PIN_DIR, LOW);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_SERVO, LOW);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT);
  saidasSeguras();
}

// Retorna 'C' ou 'S' (SAIR) quando terminar uma linha; '\0' caso contrario.
char lerComando() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      comandoBuffer[comandoPos] = '\0';
      comandoPos = 0;
      if (strcmp(comandoBuffer, "C") == 0 || strcmp(comandoBuffer, "c") == 0) return 'C';
      if (strcmp(comandoBuffer, "SAIR") == 0 || strcmp(comandoBuffer, "sair") == 0) return 'S';
      Serial.println(F("COMANDO_INVALIDO: use C ou SAIR, com Nova linha."));
      continue;
    }
    if (comandoPos < sizeof(comandoBuffer) - 1) {
      comandoBuffer[comandoPos++] = c;
    } else {
      // Descartamos linha longa demais, sem fragmentar comandos.
      comandoPos = 0;
      Serial.println(F("COMANDO_MUITO_LONGO"));
    }
  }
  return '\0';
}

bool confirmar(const __FlashStringHelper* mensagem) {
  Serial.println();
  Serial.println(mensagem);
  Serial.println(F("Digite C + ENVIAR para iniciar; SAIR + ENVIAR cancela."));
  while (true) {
    saidasSeguras();
    char c = lerComando();
    if (c == 'C') { Serial.println(F("CONFIRMADO")); return true; }
    if (c == 'S') { Serial.println(F("TESTE:CANCELADO")); return false; }
    delay(25);
  }
}

bool esperar(unsigned long tempoMs) {
  unsigned long inicio = millis();
  while ((unsigned long)(millis() - inicio) < tempoMs) {
    saidasSeguras();
    if (lerComando() == 'S') {
      Serial.println(F("TESTE:CANCELADO"));
      return false;
    }
    delay(25);
  }
  return true;
}

bool leituraAdc(long& resultado) {
  if (!balanca.wait_ready_timeout(TIMEOUT_HX_MS)) {
    Serial.println(F("HX711:ERRO:SEM_RESPOSTA"));
    return false;
  }
  resultado = balanca.read();
  return true;
}

bool efetuarTara() {
  Serial.println(F("TARA_INICIAL:REPOUSO_4S"));
  if (!esperar(REPOUSO_MS)) return false;
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_TARA; ++i) {
    long adc;
    if (!leituraAdc(adc)) return false;
    soma += (int64_t)adc;
  }
  offsetAdc = (long)(soma / (int64_t)LEITURAS_TARA);
  balanca.set_offset(offsetAdc);
  Serial.print(F("TARA:OK:OFFSET_ADC:"));
  Serial.println(offsetAdc);
  return true;
}

// Converte cada leitura individual, SEM fazer nova tara e SEM zerar negativos.
bool medirPonto(Ponto& p) {
  float soma = 0.0f;
  float somaQuadrados = 0.0f;
  float menor = 0.0f;
  float maior = 0.0f;
  int64_t somaAdc = 0;

  for (byte i = 0; i < LEITURAS_POR_PONTO; ++i) {
    long raw;
    if (!leituraAdc(raw)) return false;
    const float gramas = ((float)((int64_t)raw - (int64_t)offsetAdc)) /
                         FATOR_CONTAGENS_POR_G;
    if (i == 0 || gramas < menor) menor = gramas;
    if (i == 0 || gramas > maior) maior = gramas;
    soma += gramas;
    somaQuadrados += gramas * gramas;
    somaAdc += (int64_t)raw;
  }

  p.peso = soma / (float)LEITURAS_POR_PONTO;
  p.adc = (float)somaAdc / (float)LEITURAS_POR_PONTO;
  p.amplitude = maior - menor;
  // Desvio-padrao amostral das leituras individuais, em g.
  float variancia = (somaQuadrados -
                    ((soma * soma) / (float)LEITURAS_POR_PONTO)) /
                   (float)(LEITURAS_POR_PONTO - 1);
  if (variancia < 0.0f) variancia = 0.0f;
  p.dp = sqrtf(variancia);
  return true;
}

void imprimirResumo(const __FlashStringHelper* etiqueta, const Resumo& r) {
  Serial.println(F("----------------------------------------"));
  Serial.print(F("RESUMO:")); Serial.println(etiqueta);
  Serial.print(F("PONTOS:")); Serial.println(r.quantidade);
  Serial.print(F("PRIMEIRO_G:")); Serial.println(r.primeiro, 4);
  Serial.print(F("ULTIMO_G:")); Serial.println(r.ultimo, 4);
  Serial.print(F("MEDIA_G:")); Serial.println(r.media, 4);
  Serial.print(F("MIN_G:")); Serial.println(r.minimo, 4);
  Serial.print(F("MAX_G:")); Serial.println(r.maximo, 4);
  Serial.print(F("AMPLITUDE_AO_LONGO_DO_TEMPO_G:"));
  Serial.println(r.maximo - r.minimo, 4);
  Serial.print(F("DERIVA_FINAL_MENOS_INICIAL_G:"));
  Serial.println(r.deriva, 4);
  Serial.print(F("INCLINACAO_ESTIMADA_G_POR_MIN:"));
  Serial.println(r.inclinacaoGMin, 5);
  Serial.print(F("MAIOR_AMPLITUDE_DENTRO_DE_PONTO_G:"));
  Serial.println(r.maiorAmplitudeInterna, 4);
  Serial.print(F("MAIOR_DP_DENTRO_DE_PONTO_G:"));
  Serial.println(r.maiorDesvioPadraoInterno, 4);
  Serial.print(F("PONTOS_COM_AMPLITUDE_MAIOR_QUE_0_30G:"));
  Serial.println(r.pontosRuidoAlto);
}

bool executarFase(const __FlashStringHelper* etiqueta, Resumo& resultado) {
  resultado.valido = false;
  resultado.quantidade = 0;
  resultado.pontosRuidoAlto = 0;
  resultado.maiorAmplitudeInterna = 0;
  resultado.maiorDesvioPadraoInterno = 0;

  Serial.println(F("========================================"));
  Serial.print(F("FASE:")); Serial.println(etiqueta);
  Serial.println(F("REPOUSO_4S: nao toque na balanca"));
  if (!esperar(REPOUSO_MS)) return false;

  // Registrar no instante nominal 0, 30, ... 600 segundos.
  // A medicao dura ~1 s; 'T' representa o inicio nominal do bloco.
  const unsigned long inicio = millis();
  double soma = 0.0;
  double somaT = 0.0;
  double somaT2 = 0.0;
  double somaTG = 0.0;
  for (byte i = 0; i < TOTAL_PONTOS; ++i) {
    unsigned long marco = (unsigned long)i * INTERVALO_MS;
    while ((unsigned long)(millis() - inicio) < marco) {
      saidasSeguras();
      if (lerComando() == 'S') {
        Serial.println(F("TESTE:CANCELADO"));
        return false;
      }
      delay(25);
    }
    const unsigned long inicioRealMedicao = millis();
    Ponto p;
    if (!medirPonto(p)) {
      Serial.print(F("TESTE:ERRO:LEITURA_FASE:"));
      Serial.println(etiqueta);
      return false;
    }

    if (i == 0) {
      resultado.primeiro = p.peso;
      resultado.minimo = p.peso;
      resultado.maximo = p.peso;
    }
    if (p.peso < resultado.minimo) resultado.minimo = p.peso;
    if (p.peso > resultado.maximo) resultado.maximo = p.peso;
    if (p.amplitude > resultado.maiorAmplitudeInterna) resultado.maiorAmplitudeInterna = p.amplitude;
    if (p.dp > resultado.maiorDesvioPadraoInterno) resultado.maiorDesvioPadraoInterno = p.dp;
    if (p.amplitude > LIMITE_AMPLITUDE_G) ++resultado.pontosRuidoAlto;
    resultado.ultimo = p.peso;
    ++resultado.quantidade;

    const double tMin = ((double)(inicioRealMedicao - inicio)) / 60000.0;
    soma += p.peso;
    somaT += tMin;
    somaT2 += tMin * tMin;
    somaTG += tMin * p.peso;

    Serial.print(F("LEITURA:")); Serial.print(etiqueta);
    Serial.print(F(":PONTO:")); Serial.print(i + 1);
    Serial.print(F("/")); Serial.print(TOTAL_PONTOS);
    Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)(inicioRealMedicao - inicio) / 1000UL);
    Serial.print(F(":PESO_G:")); Serial.print(p.peso, 4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.print(p.amplitude, 4);
    Serial.print(F(":DP_G:")); Serial.print(p.dp, 4);
    Serial.print(F(":ADC_MEDIO:")); Serial.print(p.adc, 1);
    if (p.amplitude > LIMITE_AMPLITUDE_G) Serial.print(F(":AVISO:OSCILACAO"));
    Serial.println();
  }

  resultado.media = (float)(soma / (double)TOTAL_PONTOS);
  resultado.deriva = resultado.ultimo - resultado.primeiro;
  const double n = (double)TOTAL_PONTOS;
  const double denominador = (n * somaT2) - (somaT * somaT);
  resultado.inclinacaoGMin = (denominador > 0.0) ?
    (float)(((n * somaTG) - (somaT * soma)) / denominador) : 0.0f;
  resultado.valido = true;
  imprimirResumo(etiqueta, resultado);
  return true;
}

bool medirRetornoAoZero() {
  Serial.println(F("========================================"));
  Serial.println(F("RETORNO_AO_ZERO:REPOUSO_4S"));
  if (!esperar(REPOUSO_MS)) return false;
  Ponto p;
  if (!medirPonto(p)) return false;
  Serial.print(F("RESULTADO:ZERO_APOS_RETIRADA_G:")); Serial.println(p.peso, 4);
  Serial.print(F("RESULTADO:ZERO_APOS_RETIRADA_AMPLITUDE_G:")); Serial.println(p.amplitude, 4);
  Serial.print(F("RESULTADO:ZERO_APOS_RETIRADA_MENOS_ZERO_INICIAL_G:"));
  Serial.println(p.peso - vazio.primeiro, 4);
  return true;
}

void finalizar(const __FlashStringHelper* situacao) {
  saidasSeguras();
  Serial.println(F("========================================"));
  Serial.print(F("TESTE:FINALIZADO:")); Serial.println(situacao);
}

void setup() {
  // Saidas desligadas imediatamente apos reset, antes de inicializar serial/HX711.
  prepararSaidas();
  Serial.begin(9600);
  balanca.begin(PIN_DT, PIN_SCK);
  balanca.set_scale(FATOR_CONTAGENS_POR_G);

  Serial.println(F("========================================"));
  Serial.println(F("TESTE:DERIVA_HX711_V10"));
  Serial.println(F("Bandeja fixa em X durante todo o ensaio."));
  Serial.println(F("A:10min vazia / B:10min carregada / retorno ao zero."));
  Serial.println(F("21 pontos por fase, intervalo de 30 s."));
  Serial.println(F("Uma unica tara inicial; sem novas taras."));
  Serial.println(F("Servo sem PWM; nenhum pulso de motor."));
  Serial.println(F("Aquecedor fisicamente desconectado; D6 e D11 LOW."));
  Serial.print(F("FATOR_CONTAGENS_POR_G:")); Serial.println(FATOR_CONTAGENS_POR_G, 4);
  Serial.println(F("DADOS EM GRAMAS; NAO ALTERAR FATOR NESTE ENSAIO."));

  if (!confirmar(F("FASE A: deixe o prato VAZIO e PARADO em X (embaixo)."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!efetuarTara()) { finalizar(F("ERRO_TARA")); return; }
  if (!executarFase(F("A_VAZIO"), vazio)) { finalizar(F("FALHA_FASE_A")); return; }

  if (!confirmar(F("FASE B: coloque UMA MASSA FIXA (2 a 5 g de graos) no centro; nao mova a bandeja."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!executarFase(F("B_CARGA"), carregado)) { finalizar(F("FALHA_FASE_B")); return; }

  if (!confirmar(F("FINAL: retire SOMENTE os graos; mantenha o prato em X."))) {
    finalizar(F("CANCELADO")); return;
  }
  if (!medirRetornoAoZero()) { finalizar(F("FALHA_ZERO_FINAL")); return; }

  Serial.println(F("========================================"));
  Serial.println(F("RELATORIO_FINAL_DERIVA_HX711_V10"));
  Serial.print(F("DERIVA_ZERO_10_MIN_G:")); Serial.println(vazio.deriva, 4);
  Serial.print(F("TAXA_DERIVA_ZERO_G_POR_MIN:")); Serial.println(vazio.inclinacaoGMin, 5);
  Serial.print(F("DERIVA_COM_CARGA_10_MIN_G:")); Serial.println(carregado.deriva, 4);
  Serial.print(F("TAXA_DERIVA_COM_CARGA_G_POR_MIN:")); Serial.println(carregado.inclinacaoGMin, 5);
  Serial.print(F("AMPLITUDE_ZERO_10_MIN_G:")); Serial.println(vazio.maximo - vazio.minimo, 4);
  Serial.print(F("AMPLITUDE_COM_CARGA_10_MIN_G:")); Serial.println(carregado.maximo - carregado.minimo, 4);
  Serial.println(F("LEMBRETE: massa nao aferida e fator de escala provisorio."));
  Serial.println(F("SEM CORRECAO AUTOMATICA, SEM EEPROM, SEM AQUECIMENTO."));
  finalizar(F("SUCESSO"));
}

void loop() {
  // Teste executado uma unica vez por reset. Mantem as saidas seguras.
  saidasSeguras();
  delay(100);
}
