/*
 * Arduino_Grao_CNN - Ensaio de controle HX711 V10
 * 20 minutos com prato sempre VAZIO, sem tocar no equipamento.
 * Um C inicial, tara unica, leituras continuas a cada 30 s.
 * Marcadores de 5 e 10 minutos SEM confirmacao/pausa.
 * Objetivo: investigar salto de ~0,35 g observado em transicao B->C.
 *
 * ATENCAO: D6 e D11 LOW no diagnostico, como sketch anterior.
 * LOW pode LIGAR rele ativo-baixo! Desligue fisicamente a potencia
 * do aquecedor antes de usar este programa.
 * Nao use com motor/plataforma em movimento.
 */
#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

const byte PINO_DT=A1, PINO_SCK=A0;
const byte PINO_DIR=2, PINO_STEP=3, PINO_RELE=6, PINO_SERVO=9, PINO_AQUECEDOR=11;
const float FATOR_CONTAGENS_POR_G=-1129.6755f; // Provisorio; NAO alterar
const unsigned long INTERVALO_MS=30000UL;
const unsigned long TIMEOUT_ADC_MS=2500UL;
const byte N_PONTOS=41; // t=0, 30, ..., 1200 s
const byte LEITURAS_POR_PONTO=24;
const byte LEITURAS_TARA=20;
HX711 balanca;
long offsetADC=0;
float valores[N_PONTOS];

void atuadoresParados() {
  digitalWrite(PINO_DIR,LOW);
  digitalWrite(PINO_STEP,LOW);
  digitalWrite(PINO_SERVO,LOW);
  digitalWrite(PINO_RELE,LOW);
  digitalWrite(PINO_AQUECEDOR,LOW);
}
void configurarSaidas() {
  atuadoresParados();
  pinMode(PINO_DIR,OUTPUT);
  pinMode(PINO_STEP,OUTPUT);
  pinMode(PINO_SERVO,OUTPUT);
  pinMode(PINO_RELE,OUTPUT);
  pinMode(PINO_AQUECEDOR,OUTPUT);
  atuadoresParados();
}

bool lerADC(long &valor) {
  if (!balanca.wait_ready_timeout(TIMEOUT_ADC_MS)) {
    Serial.println(F("ERRO:HX711_SEM_RESPOSTA"));
    return false;
  }
  valor=balanca.read();
  return true;
}

bool medir(float &peso, float &dp, float &amplitude) {
  float media=0.0f, m2=0.0f, minimo=0.0f, maximo=0.0f;
  for(byte k=0;k<LEITURAS_POR_PONTO;k++) {
    long bruto;
    if(!lerADC(bruto)) return false;
    float g=((float)((int64_t)bruto-(int64_t)offsetADC))/FATOR_CONTAGENS_POR_G;
    if(k==0 || g<minimo) minimo=g;
    if(k==0 || g>maximo) maximo=g;
    const float delta=g-media;
    media+=delta/(float)(k+1);
    m2+=delta*(g-media);
  }
  peso=media;
  dp=sqrtf(m2/(LEITURAS_POR_PONTO-1));
  amplitude=maximo-minimo;
  return true;
}

float mediaIntervalo(byte inicio, byte fim) {
  float soma=0;
  for(byte i=inicio;i<=fim;i++) soma+=valores[i];
  return soma/(float)(fim-inicio+1);
}

void relatorio() {
  Serial.println(F("=========================================="));
  Serial.println(F("RELATORIO_CONTROLE_CONTINUO_HX711_V10"));
  Serial.print(F("ZERO_0MIN_G:")); Serial.println(valores[0],4);
  Serial.print(F("ZERO_5MIN_G:")); Serial.println(valores[10],4);
  Serial.print(F("ZERO_10MIN_G:")); Serial.println(valores[20],4);
  Serial.print(F("ZERO_20MIN_G:")); Serial.println(valores[40],4);
  Serial.print(F("DELTA_0_A_5MIN_G:")); Serial.println(valores[10]-valores[0],4);
  Serial.print(F("DELTA_5_A_10MIN_G:")); Serial.println(valores[20]-valores[10],4);
  Serial.print(F("DELTA_10_A_20MIN_G:")); Serial.println(valores[40]-valores[20],4);
  // Salto entre pontos consecutivos, cada ponto separado por 30 s.
  Serial.print(F("DELTA_JANELA_MARCO_5MIN_G:")); Serial.println(valores[11]-valores[10],4);
  Serial.print(F("DELTA_JANELA_MARCO_10MIN_G:")); Serial.println(valores[21]-valores[20],4);
  Serial.print(F("MEDIA_0_A_5MIN_G:")); Serial.println(mediaIntervalo(0,10),4);
  Serial.print(F("MEDIA_5_A_10MIN_G:")); Serial.println(mediaIntervalo(11,20),4);
  Serial.print(F("MEDIA_10_A_20MIN_G:")); Serial.println(mediaIntervalo(21,40),4);
  float maiorSalto=0.0f;
  byte indiceSalto=1;
  for(byte i=1;i<N_PONTOS;i++) {
    float delta=fabsf(valores[i]-valores[i-1]);
    if(delta>maiorSalto) { maiorSalto=delta; indiceSalto=i; }
  }
  Serial.print(F("MAIOR_SALTO_ENTRE_PONTOS_G:")); Serial.println(maiorSalto,4);
  Serial.print(F("TEMPO_MAIOR_SALTO_S:")); Serial.println((unsigned long)indiceSalto*30UL);
  Serial.println(F("SEM_SEGUNDA_TARA; SEM_CARGA; SEM_ACAO_NAS_TRANSICOES"));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void setup() {
  configurarSaidas();
  Serial.begin(9600);
  balanca.begin(PINO_DT,PINO_SCK);
  Serial.println(F("=========================================="));
  Serial.println(F("TESTE:CONTROLE_CONTINUO_HX711_V10"));
  Serial.println(F("DT=A1 SCK=A0 fator=-1129.6755 provisiorio"));
  Serial.println(F("PRATO VAZIO 20 MINUTOS; SEM TOCAR NO EQUIPAMENTO"));
  Serial.println(F("24 leituras por ponto, 30s entre pontos"));
  Serial.println(F("Aquecedor SEM alimentacao de potencia obrigatoriamente!"));
  Serial.println(F("Aqueça eletronicamente Arduino/HX711 por 30 min antes de C."));
  Serial.println(F("Quando estiver pronto, digite C e envie (Nova linha)."));

  // Aguarda apenas uma confirmacao inicial. Nunca reinicia a tara depois.
  while(true) {
    atuadoresParados();
    if(Serial.available()) {
      char c=(char)Serial.read();
      if(c=='C' || c=='c') break;
    }
    delay(20);
  }
  while(Serial.available()) Serial.read();
  Serial.println(F("CONFIRMADO"));

  for(byte i=0;i<5;i++) {
    long bruto;
    if(!lerADC(bruto)) { Serial.println(F("TESTE:ERRO_DESCARTE")); return; }
  }
  delay(4000);
  int64_t soma=0;
  for(byte i=0;i<LEITURAS_TARA;i++) {
    long bruto;
    if(!lerADC(bruto)) { Serial.println(F("TESTE:ERRO_TARA")); return; }
    soma+=(int64_t)bruto;
  }
  offsetADC=(long)(soma/LEITURAS_TARA);
  Serial.print(F("TARA_UNICA:OFFSET_ADC:")); Serial.println(offsetADC);
  delay(4000);

  Serial.println(F("FASE:CONTINUA_SEM_CARGA"));
  unsigned long inicio=millis();
  for(byte i=0;i<N_PONTOS;i++) {
    unsigned long alvo=(unsigned long)i*INTERVALO_MS;
    while((unsigned long)(millis()-inicio)<alvo) {
      atuadoresParados();
      delay(20);
    }
    float peso,dp,amplitude;
    if(!medir(peso,dp,amplitude)) { Serial.println(F("TESTE:ERRO_LEITURA")); return; }
    valores[i]=peso;
    Serial.print(F("PONTO:")); Serial.print(i+1);
    Serial.print(F("/41:TEMPO_S:")); Serial.print((unsigned long)i*30UL);
    Serial.print(F(":PESO_MEDIO_G:")); Serial.print(peso,4);
    Serial.print(F(":RUIDO_DP_G:")); Serial.print(dp,4);
    Serial.print(F(":AMPLITUDE_G:")); Serial.println(amplitude,4);
    if(i==10) Serial.println(F("MARCO:5MIN_SEM_ACAO"));
    if(i==20) Serial.println(F("MARCO:10MIN_SEM_ACAO"));
  }
  relatorio();
}
void loop() {
  atuadoresParados();
  delay(100);
}
