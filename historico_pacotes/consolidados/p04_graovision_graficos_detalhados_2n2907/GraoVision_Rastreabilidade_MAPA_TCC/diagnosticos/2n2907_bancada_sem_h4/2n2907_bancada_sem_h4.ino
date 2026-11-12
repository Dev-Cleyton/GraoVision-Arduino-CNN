/*
  2N2907 / PN2907 -- ESTUDO EXPERIMENTAL DA JUNCAO BASE-EMISSOR.
  Arduino UNO, sensor EXTERNO em A3. NAO e termometro calibrado.
  APENAS BANCADA, H4 12V FISICAMENTE DESCONECTADA.
  Pinagem E/B/C depende do FABRICANTE. CONFIRA DATASHEET DA PECA.

  Esquema eletrico (PNP conectado como diodo):
      5V ---- [10k] ----+---- A3 ---- EMISSOR (E)
                       |
                       +---- junction E-B do 2N2907
                                 BASE (B) --- GND
                               COLETOR (C) --- GND

  Nao execute ao mesmo tempo que o firmware do equipamento.
  O monitoramento com NTC A2 nao funcionara com este sketch diagnostico.
*/
const byte PIN_JUNCAO = A3;
const unsigned int AMOSTRAS=32;
float v0_mV=0;

void setup() {
  // Relés ativos LOW: inicializa como HIGH; fonte de potencia H4 deve
  // permanecer FISICAMENTE DESLIGADA, pois boot/reset pode ter transientes.
  digitalWrite(7,HIGH); pinMode(7,OUTPUT);
  digitalWrite(6,HIGH); pinMode(6,OUTPUT);
  digitalWrite(8,HIGH); pinMode(8,OUTPUT);
  digitalWrite(4,HIGH); pinMode(4,OUTPUT);
  Serial.begin(9600);
  analogReference(INTERNAL); // 1.1 V NOMINAL, tolerancia da referencia nao aferida
  delay(30);
  analogRead(PIN_JUNCAO); // descarte inicial apos configurar ADC
  Serial.println(F("2N2907:JUNCAO_EXPERIMENTAL:A3:SEM_H4"));
  Serial.println(F("JUNCAO:NAO_E_TERMOMETRO_CALIBRADO:NUNCA_USAR_COMO_CORTE_H4"));
}

float lerJuncao_mV() {
  unsigned long soma=0;
  for(unsigned int i=0;i<AMOSTRAS;i++) {
    soma+=(unsigned long)analogRead(PIN_JUNCAO);
    delay(2);
  }
  // 1100mV e aproximado. Medir/calibrar Vref melhora exatidao.
  return (soma/(float)AMOSTRAS) * (1100.0f/1023.0f);
}

void loop() {
  float mv=lerJuncao_mV();
  if (v0_mV==0.0f) v0_mV=mv;
  // Apenas diferenca aproximada, pressupondo -2mV/C; NAO grau absoluto.
  float deltaC_aprox=(v0_mV-mv)/2.0f;
  Serial.print(F("VEB_mV:"));Serial.print(mv,2);
  Serial.print(F(":DELTA_C_APROX:"));Serial.println(deltaC_aprox,1);
  delay(1000);
}
