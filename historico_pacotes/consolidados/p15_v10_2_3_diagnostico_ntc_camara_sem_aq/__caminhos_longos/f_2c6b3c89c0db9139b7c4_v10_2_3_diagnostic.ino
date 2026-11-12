/*
 * TCC GRAOS - V10.2.3 DIAGNOSTICO NTC DA CAMARA (SEM AQUECIMENTO)
 * Arduino Uno 16 MHz | Monitor Serial 9600 baud
 * Sensor: NTC 10k BETA 3950 conectado em A2
 * Divisor EXISTENTE: 5V - resistor 10k - A2 - NTC - GND.
 * Relés LOW=ON: pinos D7/D6/D8/D4 sempre HIGH/OFF.
 *
 * PERIGO: pinos podem ficar flutuantes durante reset/boot.
 * DESLIGUE FISICAMENTE o circuito de aquecimento da alimentacao.
 * Acomode/trave mecanicamente o elevador em X ANTES de carregar este
 * diagnostico: ele DESLIGA O RELE D6 e nao retém torque do motor.
 * Nao movimenta motor, servo, nem altera EEPROM. Nao controla aquecedor.
 */
#include <Arduino.h>
#include <math.h>

const uint8_t PIN_NTC = A2;
const uint8_t RELE_AQUECEDOR = 7;
const uint8_t RELE_DRIVER = 6;
const uint8_t RELE_3 = 8;
const uint8_t RELE_4 = 4;
const float RESISTOR_FIXO_OHM = 10000.0f;
const float RESISTENCIA_NTC_25C_OHM = 10000.0f;
const float BETA_NTC_K = 3950.0f;
const float TEMPERATURA_REFERENCIA_K = 298.15f;
const unsigned long INTERVALO_MS = 2000UL;
unsigned long ultimaLeituraMs = 0;

void desativarSaidas() {
  // Precarrega o latch de saida em HIGH ANTES de configurar como saida.
  digitalWrite(RELE_AQUECEDOR, HIGH);
  digitalWrite(RELE_DRIVER, HIGH);
  digitalWrite(RELE_3, HIGH);
  digitalWrite(RELE_4, HIGH);
  pinMode(RELE_AQUECEDOR, OUTPUT);
  pinMode(RELE_DRIVER, OUTPUT);
  pinMode(RELE_3, OUTPUT);
  pinMode(RELE_4, OUTPUT);
}

float adcMedio() {
  long soma = 0;
  for (uint8_t i = 0; i < 16; i++) {
    soma += analogRead(PIN_NTC);
    delay(3);
  }
  return (float)soma / 16.0f;
}

void medirTemperatura() {
  const float adc = adcMedio();
  Serial.print(F("NTC_CAMARA:"));
  Serial.print(millis());
  Serial.print(':');
  Serial.print(adc, 1);
  Serial.print(':');

  if (adc <= 1.0f || adc >= 1022.0f) {
    Serial.println(F("ERRO:ADC_LIMITE_VERIFICAR_NTC_CABOS_DIVISOR"));
    return;
  }

  // Resistor FIXO entre 5V e A2; NTC entre A2 e GND.
  const float rNtc = RESISTOR_FIXO_OHM * adc / (1023.0f - adc);
  const float inversoKelvin = (1.0f / TEMPERATURA_REFERENCIA_K)
       + log(rNtc / RESISTENCIA_NTC_25C_OHM) / BETA_NTC_K;
  if (!(inversoKelvin > 0.0f)) {
    Serial.println(F("ERRO:CALCULO_TEMPERATURA"));
    return;
  }
  const float tempC = (1.0f / inversoKelvin) - 273.15f;
  if (isnan(tempC) || isinf(tempC)) {
    Serial.println(F("ERRO:CALCULO_TEMPERATURA"));
    return;
  }
  Serial.print(rNtc, 1);
  Serial.print(':');
  Serial.print(tempC, 2);
  Serial.println(F(":LEITURA_NAO_CALIBRADA"));
}

void setup() {
  desativarSaidas();
  Serial.begin(9600);
  Serial.println(F("V10.2.3:DIAGNOSTICO_NTC_CAMARA_SEM_AQUECIMENTO"));
  Serial.println(F("AQUECEDOR:DESLIGADO_FISICAMENTE_OBRIGATORIO"));
  Serial.println(F("ATUADORES:DESABILITADOS_SEM_MOVIMENTO"));
  Serial.println(F("NTC:A2:10K:BETA3950:FIXO_10K_AO_5V"));
  Serial.println(F("FORMATO:NTC_CAMARA:MS:ADC_MEDIO:RESISTENCIA_OHM:TEMPERATURA_C:STATUS"));
  medirTemperatura();
  ultimaLeituraMs = millis();
}

void loop() {
  // Nunca habilitar o aquecedor nem o driver. Nenhum comando serial atua neles.
  desativarSaidas();
  if ((unsigned long)(millis() - ultimaLeituraMs) >= INTERVALO_MS) {
    ultimaLeituraMs = millis();
    medirTemperatura();
  }
}
