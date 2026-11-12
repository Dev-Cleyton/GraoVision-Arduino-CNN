/*
 * TCC - Arduino_Grao_CNN - V10
 * VALIDACAO DE CONSERVACAO DE MASSA ENTRE ALTURAS X/Y
 *
 * Ensaio dedicado, SEM camera, SEM aquecimento, SEM PWM do servo.
 * ATENCAO: desconecte FISICAMENTE a alimentacao da resistencia/aquecedor.
 * Os valores LOW nos pinos D6/D11 presumem acionamento ativo-alto.
 *
 * Arduino UNO: HX711 DT=A1, SCK=A0; motor DIR=D2, STEP=D3;
 * rele=D6, aquecimento=D11, servo D9 (sem PWM).
 * Fator HX711 provisiorio -1129.6755 (250 g nominais).
 *
 * 1 tara inicial em X (prato vazio); nao refaz tara.
 * 1) Zeros ANTES: X vazio -> Y vazio -> X vazio
 * 2) Coloca a MESMA carga UMA UNICA VEZ em X
 * 3) 3 ciclos: X carga -> Y carga -> X carga, SEM retirar carga
 * 4) Retira a carga em X
 * 5) Zeros DEPOIS: X vazio -> Y vazio -> X vazio
 *
 * Cada acao depende de confirmacao do operador. SEM fim de curso,
 * SEM sensor de posicao. O movimento usa 14000 passos (firmware V10).
 * Use somente se 14000 passos ja foram comprovadamente seguros e os
 * limites mecanicos forem observados. Movimento nao pode ser cancelado
 * pela serial enquanto executa: mantenha corte de energia acessivel.
 *
 * O relatorio compara as leituras brutas em g e as leituras com
 * CORRECAO DIAGNOSTICA de zero por interpolacao linear no tempo.
 * Nao grava EEPROM e NAO muda a calibracao do sistema principal.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>

const byte HX_DT = A1;
const byte HX_SCK = A0;
const byte PIN_DIR = 2;
const byte PIN_STEP = 3;
const byte PIN_RELE = 6;
const byte PIN_HEATER = 11;
const byte PIN_SERVO = 9;

const float FATOR_CONTAGENS_POR_G = -1129.6755f;
const long PASSOS_X_Y = 14000L;
const unsigned int PULSO_US = 300;
const unsigned long REPOUSO_MS = 4000UL;
const unsigned long TIMEOUT_HX_MS = 2500UL;
const byte AMOSTRAS_POR_JANELA = 12;
const byte LEITURAS_DESCARTE = 4;
const byte MAX_JANELAS = 8;
const float AMPLITUDE_MAX_G = 0.30f;
const float DIFERENCA_MEDIAS_MAX_G = 0.12f;
const byte NUM_CICLOS = 3;

HX711 escala;

struct Leitura {
  float gramas;
  unsigned long instante;
};

struct Repeticao {
  Leitura xAntes;
  Leitura y;
  Leitura xDepois;
};

Repeticao ciclos[NUM_CICLOS];
Leitura xZeroInicial, yZeroInicial, xZeroRetornoInicial;
Leitura xZeroFinal, yZeroFinal, xZeroRetornoFinal;

void saidasDesligadas() {
  // Nao ligar aquecimento ou rele sob nenhuma circunstancia.
  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_SERVO, LOW);  // nenhum PWM de servo neste sketch
}

void encerrarComFalha(const __FlashStringHelper *texto) {
  saidasDesligadas();
  digitalWrite(PIN_STEP, LOW);
  Serial.print(F("TESTE:INTERROMPIDO:"));
  Serial.println(texto);
}

bool confirmar(const char *esperado) {
  Serial.print(F("Digite "));
  Serial.print(esperado);
  Serial.println(F(" e pressione ENVIAR (SAIR cancela na espera)."));
  while (true) {
    saidasDesligadas();
    if (Serial.available() == 0) { delay(30); continue; }
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd.length() == 0) continue;
    if (cmd == "SAIR") {
      encerrarComFalha(F("CANCELADO_PELO_USUARIO"));
      return false;
    }
    if (cmd.equalsIgnoreCase(esperado)) {
      Serial.println(F("CONFIRMADO"));
      return true;
    }
    Serial.println(F("COMANDO_INVALIDO: confira a etapa."));
  }
}

bool hxPronto() {
  if (!escala.wait_ready_timeout(TIMEOUT_HX_MS)) {
    Serial.println(F("HX711:ERRO:TIMEOUT"));
    return false;
  }
  return true;
}

bool tareEmX() {
  Serial.println(F("TARA: pratos vazio, sistema em X. Acomodacao 4 s."));
  delay(REPOUSO_MS);
  int64_t soma = 0;
  const byte N = 20;
  for (byte i = 0; i < N; i++) {
    if (!hxPronto()) return false;
    soma += escala.read();
  }
  escala.set_offset((long)(soma / N));
  Serial.println(F("TARA:OK:UNICA"));
  return true;
}

bool medir(Leitura &valor) {
  saidasDesligadas();
  Serial.println(F("HX711:ACOMODACAO_4S"));
  delay(REPOUSO_MS);
  for (byte i = 0; i < LEITURAS_DESCARTE; i++) {
    if (!hxPronto()) return false;
    (void)escala.read();
  }
  bool janelaValidaAnterior = false;
  float mediaAnterior = 0.0f;
  for (byte j = 1; j <= MAX_JANELAS; j++) {
    float soma = 0.0f, minimo = 0.0f, maximo = 0.0f;
    for (byte i = 0; i < AMOSTRAS_POR_JANELA; i++) {
      if (!hxPronto()) return false;
      const float g = escala.get_units(1);
      if (isnan(g) || isinf(g)) {
        Serial.println(F("HX711:ERRO:LEITURA_INVALIDA"));
        return false;
      }
      if (i == 0 || g < minimo) minimo = g;
      if (i == 0 || g > maximo) maximo = g;
      soma += g;
      delay(100);
    }
    const float media = soma / AMOSTRAS_POR_JANELA;
    const float amplitude = maximo - minimo;
    Serial.print(F("HX711:JANELA:")); Serial.print(j);
    Serial.print(F(":MEDIA:")); Serial.print(media, 4);
    Serial.print(F(":AMPLITUDE:")); Serial.println(amplitude, 4);
    if (amplitude <= AMPLITUDE_MAX_G) {
      if (janelaValidaAnterior && fabsf(media - mediaAnterior) <= DIFERENCA_MEDIAS_MAX_G) {
        valor.gramas = (mediaAnterior + media) / 2.0f;
        valor.instante = millis();
        Serial.println(F("HX711:ESTAVEL"));
        return true;
      }
      mediaAnterior = media;
      janelaValidaAnterior = true;
    } else {
      janelaValidaAnterior = false;
    }
    // INSTAVEL aqui significa 'ainda nao confirmada em 2 janelas',
    // mesmo que a amplitude individual ja esteja no limite.
    Serial.print(F("HX711:AGUARDANDO_CONFIRMACAO:AMPLITUDE:"));
    Serial.println(amplitude, 4);
  }
  Serial.println(F("HX711:ERRO:SEM_ESTABILIDADE"));
  return false;
}

bool registrar(const __FlashStringHelper *etapa, Leitura &valor) {
  while (true) {
    Serial.print(F("ETAPA:")); Serial.println(etapa);
    if (medir(valor)) {
      Serial.print(F("RESULTADO:")); Serial.print(etapa);
      Serial.print(':'); Serial.println(valor.gramas, 4);
      return true;
    }
    Serial.println(F("MEDICAO_FALHOU: nao alteramos a tara. Verifique o sensor."));
    if (!confirmar("R")) return false;
  }
}

// Movimento somente apos comando do operador. Motor sem fim de curso.
void mover(bool subir) {
  saidasDesligadas();
  digitalWrite(PIN_DIR, subir ? HIGH : LOW);  // direcao da V10 anterior
  delay(10);
  for (long i = 0; i < PASSOS_X_Y; i++) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(PULSO_US);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(PULSO_US);
  }
  digitalWrite(PIN_STEP, LOW);
  Serial.print(F("MOVIMENTO:PASSOS_COMANDADOS:"));
  Serial.println(PASSOS_X_Y);
  Serial.println(F("ATENCAO: nenhum sensor confirma que o destino foi atingido."));
}

bool irPara(bool subir) {
  if (subir) {
    Serial.println(F("AUTORIZAR SUBIDA X->Y? CONFIRA CURSO LIVRE."));
  } else {
    Serial.println(F("AUTORIZAR DESCIDA Y->X? CONFIRA CURSO LIVRE."));
  }
  if (!confirmar(subir ? "SUBIR" : "DESCER")) return false;
  mover(subir);
  Serial.println(subir ? F("CONFIRME: chegada real em Y, motor parado.") :
                         F("CONFIRME: chegada real em X, motor parado."));
  return confirmar("C");
}

float zeroInterpolado(const Leitura &inicio, const Leitura &fim,
                      unsigned long instante) {
  // Interpolacao apenas diagnostica: modelo de deriva linear no tempo.
  // Usa millis() que nao deve reiniciar no ensaio.
  unsigned long duracao = fim.instante - inicio.instante;
  if (duracao == 0UL) return inicio.gramas;
  float frac = ((float)(instante - inicio.instante)) / ((float)duracao);
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  return inicio.gramas + frac * (fim.gramas - inicio.gramas);
}

void relatorio() {
  const float offInicio = yZeroInicial.gramas - xZeroRetornoInicial.gramas;
  const float offFim = yZeroFinal.gramas - xZeroFinal.gramas;
  Serial.println(F("============================================"));
  Serial.println(F("RELATORIO_FINAL_CONSERVACAO_V10"));
  Serial.print(F("ZERO_X_INICIAL:")); Serial.println(xZeroInicial.gramas, 4);
  Serial.print(F("ZERO_Y_INICIAL:")); Serial.println(yZeroInicial.gramas, 4);
  Serial.print(F("ZERO_X_APOS_RETORNO_INICIAL:")); Serial.println(xZeroRetornoInicial.gramas, 4);
  Serial.print(F("ZERO_X_FINAL:")); Serial.println(xZeroFinal.gramas, 4);
  Serial.print(F("ZERO_Y_FINAL:")); Serial.println(yZeroFinal.gramas, 4);
  Serial.print(F("ZERO_X_APOS_RETORNO_FINAL:")); Serial.println(xZeroRetornoFinal.gramas, 4);
  Serial.print(F("OFFSET_Y_X_INICIAL_APROX:")); Serial.println(offInicio, 4);
  Serial.print(F("OFFSET_Y_X_FINAL_APROX:")); Serial.println(offFim, 4);
  Serial.print(F("DERIVA_X_SEM_CARGA:"));
  Serial.println(xZeroFinal.gramas - xZeroRetornoInicial.gramas, 4);
  Serial.print(F("DERIVA_Y_SEM_CARGA:"));
  Serial.println(yZeroFinal.gramas - yZeroInicial.gramas, 4);

  float somaDif = 0.0f, somaDifAbs = 0.0f, maiorDifAbs = 0.0f;
  float somaVolta = 0.0f, maiorVoltaAbs = 0.0f;
  for (byte i = 0; i < NUM_CICLOS; i++) {
    Repeticao &c = ciclos[i];
    const float zx1 = zeroInterpolado(xZeroRetornoInicial, xZeroFinal, c.xAntes.instante);
    const float zy = zeroInterpolado(yZeroInicial, yZeroFinal, c.y.instante);
    const float zx2 = zeroInterpolado(xZeroRetornoInicial, xZeroFinal, c.xDepois.instante);
    const float mx1 = c.xAntes.gramas - zx1;
    const float my = c.y.gramas - zy;
    const float mx2 = c.xDepois.gramas - zx2;
    const float xReferencia = (mx1 + mx2) / 2.0f;
    const float dif = my - xReferencia;
    const float volta = mx2 - mx1;
    const float difAbs = fabsf(dif);
    const float voltaAbs = fabsf(volta);
    somaDif += dif;
    somaDifAbs += difAbs;
    somaVolta += volta;
    if (difAbs > maiorDifAbs) maiorDifAbs = difAbs;
    if (voltaAbs > maiorVoltaAbs) maiorVoltaAbs = voltaAbs;
    Serial.println(F("--------------------------------------------"));
    Serial.print(F("CICLO:")); Serial.println(i + 1);
    Serial.print(F("X_ANTES_BRUTO_G:")); Serial.println(c.xAntes.gramas, 4);
    Serial.print(F("Y_BRUTO_G:")); Serial.println(c.y.gramas, 4);
    Serial.print(F("X_DEPOIS_BRUTO_G:")); Serial.println(c.xDepois.gramas, 4);
    Serial.print(F("ZERO_X_INTERPOLADO_ANTES:")); Serial.println(zx1, 4);
    Serial.print(F("ZERO_Y_INTERPOLADO:")); Serial.println(zy, 4);
    Serial.print(F("ZERO_X_INTERPOLADO_DEPOIS:")); Serial.println(zx2, 4);
    Serial.print(F("MASSA_X_ANTES_CORRIGIDA:")); Serial.println(mx1, 4);
    Serial.print(F("MASSA_Y_CORRIGIDA:")); Serial.println(my, 4);
    Serial.print(F("MASSA_X_DEPOIS_CORRIGIDA:")); Serial.println(mx2, 4);
    Serial.print(F("ERRO_Y_MENOS_MEDIA_X_CORRIGIDO:")); Serial.println(dif, 4);
    Serial.print(F("RETORNO_CARGA_X_DEPOIS_MENOS_ANTES:")); Serial.println(volta, 4);
    Serial.print(F("DIFERENCA_BRUTA_Y_MENOS_X_ANTES:"));
    Serial.println(c.y.gramas - c.xAntes.gramas, 4);
  }
  Serial.println(F("============================================"));
  Serial.print(F("DIFERENCA_MEDIA_Y_X_CORRIGIDA:")); Serial.println(somaDif / NUM_CICLOS, 4);
  Serial.print(F("DIFERENCA_ABSOLUTA_MEDIA_Y_X:")); Serial.println(somaDifAbs / NUM_CICLOS, 4);
  Serial.print(F("MAIOR_DIFERENCA_ABSOLUTA_Y_X:")); Serial.println(maiorDifAbs, 4);
  Serial.print(F("RETORNO_MEDIO_COM_CARGA_X:")); Serial.println(somaVolta / NUM_CICLOS, 4);
  Serial.print(F("MAIOR_VARIACAO_RETORNO_X_COM_CARGA:")); Serial.println(maiorVoltaAbs, 4);
  Serial.println(F("INTERPOLACAO_DIAGNOSTICA; NAO E CORRECAO VALIDADA."));
  Serial.println(F("SEM AQUECIMENTO; SEM EEPROM; MASSA IDENTICA NOS 3 CICLOS."));
  Serial.println(F("FIM:TESTE_CONSERVACAO_V10"));
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(400);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  saidasDesligadas();
  escala.begin(HX_DT, HX_SCK);
  escala.set_scale(FATOR_CONTAGENS_POR_G);

  Serial.println(F("============================================"));
  Serial.println(F("VALIDACAO CONSERVACAO DE MASSA - V10"));
  Serial.println(F("AQUECIMENTO: DESLIGADO; DESCONECTE FISICAMENTE O AQUECEDOR."));
  Serial.println(F("SERVO: SEM PWM; MOVIMENTO VERTICAL SOB CONFIRMACAO."));
  Serial.println(F("INICIO OBRIGATORIO: bandeja VAZIA EM X (INFERIOR)."));
  Serial.println(F("Curso X/Y: 14000 passos; SEM sensor de fim de curso."));
  Serial.println(F("Fator HX711: -1129.6755 provisiorio."));
  Serial.println(F("Uma unica tara inicial; nao retirar a carga entre X/Y."));
  if (!hxPronto()) { encerrarComFalha(F("HX711_SEM_RESPOSTA")); return; }

  Serial.println(F("Bandeja VAZIA e parada em X? Percurso livre?"));
  if (!confirmar("C")) return;
  if (!tareEmX()) { encerrarComFalha(F("TARA_FALHOU")); return; }

  Serial.println(F("ZEROS ANTES - X VAZIO"));
  if (!registrar(F("ZERO_X_INICIAL"), xZeroInicial)) return;
  if (!irPara(true)) return;
  Serial.println(F("ZEROS ANTES - Y VAZIO"));
  if (!registrar(F("ZERO_Y_INICIAL"), yZeroInicial)) return;
  if (!irPara(false)) return;
  Serial.println(F("ZEROS ANTES - X VAZIO APOS RETORNO"));
  if (!registrar(F("ZERO_X_RETORNO_INICIAL"), xZeroRetornoInicial)) return;

  Serial.println(F("COLOQUE A MASSA UMA UNICA VEZ EM X."));
  Serial.println(F("NAO ACRESCENTE OU RETIRE GRAOS DURANTE OS 3 CICLOS."));
  Serial.println(F("Confirme que a amostra nao derrama nos movimentos."));
  if (!confirmar("C")) return;

  for (byte i = 0; i < NUM_CICLOS; i++) {
    Serial.println(F("============================================"));
    Serial.print(F("CICLO_COM_MESMA_CARGA:")); Serial.print(i + 1);
    Serial.print('/'); Serial.println(NUM_CICLOS);
    if (!registrar(F("X_CARGA_ANTES"), ciclos[i].xAntes)) return;
    if (!irPara(true)) return;
    if (!registrar(F("Y_CARGA"), ciclos[i].y)) return;
    if (!irPara(false)) return;
    if (!registrar(F("X_CARGA_DEPOIS"), ciclos[i].xDepois)) return;
    Serial.println(F("NENHUMA CARGA DEVE TER SIDO RETIRADA."));
  }

  Serial.println(F("RETIRE TODA A MASSA EM X. MANTENHA A BANDEJA."));
  if (!confirmar("C")) return;
  if (!registrar(F("ZERO_X_FINAL"), xZeroFinal)) return;
  if (!irPara(true)) return;
  if (!registrar(F("ZERO_Y_FINAL"), yZeroFinal)) return;
  if (!irPara(false)) return;
  if (!registrar(F("ZERO_X_RETORNO_FINAL"), xZeroRetornoFinal)) return;
  relatorio();
}

void loop() {
  saidasDesligadas();
  delay(250);
}
