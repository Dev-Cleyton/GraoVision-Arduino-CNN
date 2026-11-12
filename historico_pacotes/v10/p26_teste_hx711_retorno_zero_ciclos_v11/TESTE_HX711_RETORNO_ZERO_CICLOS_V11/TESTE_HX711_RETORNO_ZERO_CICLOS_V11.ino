#include <Arduino.h>
#include <math.h>
#include <ctype.h>

// V11: teste diagnostico sem cargas de potencia.
const uint8_t PIN_DT=A1, PIN_SCK=A0, PIN_NTC=A2;
const uint8_t RELES[]={7,6,8,4};
const float FATOR=1058.3167f;
const float VCC=5.0f, R_FIXO=10000.0f, R0=10000.0f;
const float BETA=3950.0f, T0=298.15f;
const uint8_t N_HX=24, N_NTC=16;
const unsigned long INTERVALO_MS=30000UL;
const unsigned long TIMEOUT_HX_MS=1500UL;
const float CARGA_MIN=25.0f, CARGA_MAX=50.0f;
long offsetTara=0;
bool abortado=false;

struct Ponto { float peso, dp, amplitude, adc, ntc, temp; bool tempOK; };
struct Resumo {
 const char* nome; uint8_t n; float inicial, final, soma;
 float tempSoma,tempMin,tempMax,tempIni,tempFim; uint8_t tempN;
};
Resumo res[7];
float zeroAntes[3], zeroDepoisIni[3], zeroDepoisFim[3];
float cargaIni[3], cargaFim[3];

void relesOff(){for(uint8_t i=0;i<4;i++) digitalWrite(RELES[i],HIGH);}
void imprimirReles(){Serial.println(F("RELES:IN1_D7=HIGH:IN2_D6=HIGH:IN3_D8=HIGH:IN4_D4=HIGH"));}
void abortar(){abortado=true;relesOff();Serial.println(F("TESTE:ABORTADO:RELES_OFF"));}
void verificarSerial(){
 static char linha[12]; static uint8_t n=0;
 while(Serial.available()){
  char c=Serial.read();
  if(c=='\n'||c=='\r'){
   linha[n]=0;
   if(n && (toupper(linha[0])=='S' || (toupper(linha[0])=='A' && toupper(linha[1])=='B'))) abortar();
   n=0;
  } else if(n<sizeof(linha)-1) linha[n++]=c;
 }
}
bool aguardarMs(unsigned long ms){unsigned long ini=millis();while(millis()-ini<ms){verificarSerial();if(abortado)return false;delay(10);}return true;}
bool confirmar(const __FlashStringHelper* mensagem){
 Serial.println();Serial.println(mensagem);
 Serial.println(F("Digite C + ENVIAR (Nova linha); S aborta."));
 char linha[12];uint8_t n=0;
 while(!abortado){
  if(Serial.available()){
   char c=Serial.read();
   if(c=='\n'||c=='\r'){
    linha[n]=0;
    if(n){char x=toupper(linha[0]);n=0;
     if(x=='C'){Serial.println(F("CONFIRMADO"));return true;}
     if(x=='S'){abortar();return false;}
     Serial.println(F("COMANDO_INVALIDO:USE_C_OU_S"));
    }
   } else if(n<sizeof(linha)-1) linha[n++]=c;
  }
  delay(10);
 }
 return false;
}
bool hxPronto(){return digitalRead(PIN_DT)==LOW;}
bool lerHx(long &valor){
 unsigned long ini=millis();
 while(!hxPronto()){
  verificarSerial();if(abortado)return false;
  if(millis()-ini>TIMEOUT_HX_MS){Serial.println(F("ERRO:HX711_TIMEOUT"));abortar();return false;}
  delay(1);
 }
 uint32_t dado=0;
 noInterrupts();
 for(uint8_t i=0;i<24;i++){
  digitalWrite(PIN_SCK,HIGH);
  delayMicroseconds(1);
  dado=(dado<<1)|(digitalRead(PIN_DT)?1UL:0UL);
  digitalWrite(PIN_SCK,LOW);
  delayMicroseconds(1);
 }
 // Pulso 25: canal A, ganho 128.
 digitalWrite(PIN_SCK,HIGH);delayMicroseconds(1);
 digitalWrite(PIN_SCK,LOW);
 interrupts();
 if(dado & 0x800000UL)dado|=0xFF000000UL;
 valor=(int32_t)dado;
 return true;
}
bool lerPonto(Ponto &p){
 double soma=0,soma2=0;long minimo=2147483647L,maximo=(-2147483647L-1L);
 for(uint8_t i=0;i<N_HX;i++){
  long v;if(!lerHx(v))return false;
  soma+=v;soma2+=(double)v*v;
  if(v<minimo)minimo=v;if(v>maximo)maximo=v;
 }
 p.adc=soma/N_HX;
 p.peso=(p.adc-offsetTara)/FATOR;
 double variancia=(soma2-soma*soma/N_HX)/(N_HX-1);
 if(variancia<0)variancia=0;
 p.dp=sqrt(variancia)/fabs(FATOR);
 p.amplitude=((double)maximo-minimo)/fabs(FATOR);
 unsigned long somaNTC=0;
 for(uint8_t i=0;i<N_NTC;i++){verificarSerial();if(abortado)return false;somaNTC+=analogRead(PIN_NTC);delay(2);}
 p.ntc=(float)somaNTC/N_NTC;
 p.tempOK=p.ntc>1 && p.ntc<1022;
 p.temp=NAN;
 if(p.tempOK){
  // NTC ao GND, resistor fixo ao +5 V (divisor de tensao).
  float r=R_FIXO*p.ntc/(1023.0f-p.ntc);
  p.temp=1.0f/(1.0f/T0+log(r/R0)/BETA)-273.15f;
  p.tempOK=isfinite(p.temp);
 }
 return true;
}
void iniciarResumo(uint8_t idx,const char* nome){
 Resumo &r=res[idx];r.nome=nome;r.n=0;r.inicial=0;r.final=0;r.soma=0;
 r.tempSoma=0;r.tempMin=1000;r.tempMax=-1000;r.tempIni=NAN;r.tempFim=NAN;r.tempN=0;
}
void registrar(uint8_t idx,const Ponto &p){
 Resumo &r=res[idx];if(!r.n)r.inicial=p.peso;r.final=p.peso;r.soma+=p.peso;r.n++;
 if(p.tempOK){if(!r.tempN)r.tempIni=p.temp;r.tempFim=p.temp;
 r.tempSoma+=p.temp;r.tempN++;if(p.temp<r.tempMin)r.tempMin=p.temp;if(p.temp>r.tempMax)r.tempMax=p.temp;}
}
void imprimirPonto(const char* nome,uint8_t i,uint8_t total,unsigned long tempo,const Ponto &p){
 Serial.print(F("PONTO:"));Serial.print(nome);
 Serial.print(':');Serial.print(i);Serial.print('/');Serial.print(total);
 Serial.print(F(":TEMPO_S:"));Serial.print(tempo/1000UL);
 Serial.print(F(":PESO_MEDIO_G:"));Serial.print(p.peso,4);
 Serial.print(F(":RUIDO_DP_G:"));Serial.print(p.dp,4);
 Serial.print(F(":AMPLITUDE_G:"));Serial.print(p.amplitude,4);
 Serial.print(F(":ADC_MEDIO:"));Serial.print(p.adc,1);
 Serial.print(F(":NTC_ADC:"));Serial.print(p.ntc,1);
 Serial.print(F(":TEMP_NTC_C:"));if(p.tempOK)Serial.print(p.temp,2);else Serial.print(F("INVALIDA"));
 Serial.print(F(":TEMP_STATUS:"));Serial.println(p.tempOK?F("OK"):F("INVALIDA"));
}
bool fase(uint8_t idx,const char* nome,unsigned long duracao){
 iniciarResumo(idx,nome);Serial.print(F("FASE:"));Serial.println(nome);
 uint8_t total=(uint8_t)(duracao/INTERVALO_MS+1);
 unsigned long inicio=millis();
 for(uint8_t i=0;i<total;i++){
  unsigned long alvo=inicio+(unsigned long)i*INTERVALO_MS;
  while((long)(millis()-alvo)<0){verificarSerial();if(abortado)return false;delay(10);}
  Ponto p;if(!lerPonto(p))return false;
  registrar(idx,p);imprimirPonto(nome,i+1,total,(unsigned long)i*INTERVALO_MS,p);
 }
 return true;
}
void resumo(uint8_t idx){
 const Resumo &r=res[idx];if(!r.n)return;
 Serial.println(F("----------------------------------------"));
 Serial.print(F("RESUMO:"));Serial.println(r.nome);
 Serial.print(F("PONTOS:"));Serial.println(r.n);
 Serial.print(F("PESO_INICIAL_G:"));Serial.println(r.inicial,4);
 Serial.print(F("PESO_FINAL_G:"));Serial.println(r.final,4);
 Serial.print(F("DELTA_PESO_G:"));Serial.println(r.final-r.inicial,4);
 Serial.print(F("MEDIA_PESO_G:"));Serial.println(r.soma/r.n,4);
 Serial.print(F("TEMP_PONTOS_VALIDOS:"));Serial.println(r.tempN);
 if(r.tempN){
  Serial.print(F("TEMP_MEDIA_C:"));Serial.println(r.tempSoma/r.tempN,4);
  Serial.print(F("TEMP_MIN_C:"));Serial.println(r.tempMin,4);
  Serial.print(F("TEMP_MAX_C:"));Serial.println(r.tempMax,4);
  Serial.print(F("TEMP_VARIACAO_MAX_MIN_C:"));Serial.println(r.tempMax-r.tempMin,4);
  Serial.print(F("TEMP_DELTA_INICIO_FIM_C:"));Serial.println(r.tempFim-r.tempIni,4);
 }
}
void comparacao(const char* nome,float v){Serial.print(nome);Serial.println(v,4);}
void relatorio(){
 Serial.println(F("=========================================="));
 Serial.println(F("RELATORIO_HX711_RETORNO_ZERO_CICLOS_V11"));
 Serial.println(F("HX711:A1/A0; NTC:A2; FATOR:1058.3167"));
 Serial.println(F("TARA_UNICA:SIM; 3_CICLOS_5MIN_CARGA_10MIN_ZERO"));
 imprimirReles();
 Serial.println(F("RELES_CONECTADOS_MAS_SEMPRE_OFF; SEM_CARGAS_DE_POTENCIA"));
 Serial.print(F("OFFSET_TARA_ADC:"));Serial.println(offsetTara);
 for(uint8_t i=0;i<7;i++)resumo(i);
 Serial.println(F("---------- COMPARATIVO DOS CICLOS ----------"));
 for(uint8_t c=0;c<3;c++){
  Serial.print(F("CICLO:"));Serial.println(c+1);
  comparacao("ZERO_ANTES_CICLO_G:",zeroAntes[c]);
  comparacao("ZERO_APOS_RETIRADA_INICIAL_G:",zeroDepoisIni[c]);
  comparacao("ZERO_APOS_RETIRADA_FINAL_G:",zeroDepoisFim[c]);
  comparacao("RESIDUO_INICIAL_VS_ANTES_G:",zeroDepoisIni[c]-zeroAntes[c]);
  comparacao("RESIDUO_FINAL_VS_ANTES_G:",zeroDepoisFim[c]-zeroAntes[c]);
  comparacao("RECUPERACAO_ZERO_10MIN_G:",zeroDepoisFim[c]-zeroDepoisIni[c]);
  comparacao("CARGA_INICIAL_G:",cargaIni[c]);
  comparacao("CARGA_FINAL_G:",cargaFim[c]);
 }
 comparacao("RESIDUO_FINAL_C3_VS_BASE_ORIGINAL_G:",zeroDepoisFim[2]-res[0].final);
 Serial.println(F("NOTA: DELTAS INCLUEM DERIVA TEMPORAL E POSSIVEL HISTERESE."));
 Serial.println(F("NOTA: NTC MEDE TEMPERATURA LOCAL, NAO DA CAMARA."));
 Serial.println(F("SEM_COMPENSACAO; SEM_NOVA_TARA; SEM_EEPROM; SEM_AQUECIMENTO"));
 Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}
void setup(){
 // Escreve latch HIGH antes de habilitar os pinos como saida.
 for(uint8_t i=0;i<4;i++){digitalWrite(RELES[i],HIGH);pinMode(RELES[i],OUTPUT);}
 pinMode(PIN_DT,INPUT);pinMode(PIN_SCK,OUTPUT);digitalWrite(PIN_SCK,LOW);
 pinMode(PIN_NTC,INPUT);
 Serial.begin(9600);
 Serial.println(F("TESTE_HX711_RETORNO_ZERO_CICLOS_V11"));
 Serial.println(F("HX711 DT=A1 SCK=A0 | NTC=A2 | FATOR=1058.3167"));
 imprimirReles();
 Serial.println(F("TODOS OS RELES PERMANECEM OFF=HIGH"));
 Serial.println(F("UMA TARA; BASE VAZIO 2MIN; 3 CICLOS 5MIN+10MIN"));
 Serial.println(F("24 LEITURAS HX711 + 16 NTC; 30S ENTRE PONTOS"));
 Serial.println(F("ATENCAO: CONTATOS COM/NO/NC SEM CARGAS DE POTENCIA."));
 Serial.println(F("AQUECEDOR E DRIVER SEM ALIMENTACAO DE POTENCIA."));
 Serial.println(F("RESET PODE ACIONAR RELES ANTES DO setup()."));
 Serial.println(F("NTC NA CELULA NAO PROTEGE A CAMARA DE AQUECIMENTO."));
 Serial.println(F("MONITOR SERIAL: 9600 BAUD / NOVA LINHA / S ABORTA."));
 Serial.println(F("ESTABILIZE ARDUINO E HX711 POR 30 MIN ANTES DE CONFIRMAR."));
 if(!confirmar(F("CONFIRME: reles conectados, SEM CARGAS; prato vazio e plataforma fixa.")))return;
 Serial.println(F("HX711:DESCARTE_INICIAL:5"));
 for(uint8_t i=0;i<5;i++){long v;if(!lerHx(v))return;}
 Serial.println(F("TARA:REPOUSO_4S"));if(!aguardarMs(4000))return;
 double soma=0;
 for(uint8_t i=0;i<48;i++){long v;if(!lerHx(v))return;soma+=v;}
 offsetTara=(long)lround(soma/48.0);
 Serial.print(F("TARA_UNICA:OK:OFFSET_ADC:"));Serial.println(offsetTara);
 if(!fase(0,"A_VAZIO",120000UL))return;
 for(uint8_t c=0;c<3;c++){
  Serial.print(F("CICLO:"));Serial.print(c+1);Serial.println(F("/3"));
  if(!confirmar(F("Coloque a massa FIXA ~36 g, centralizada. Nao mova a plataforma.")))return;
  Serial.println(F("ACOMODACAO_CARGA:8S"));if(!aguardarMs(8000))return;
  Ponto validacao;if(!lerPonto(validacao))return;
  Serial.print(F("VALIDACAO_CARGA_G:"));Serial.println(validacao.peso,4);
  if(validacao.peso<CARGA_MIN || validacao.peso>CARGA_MAX){Serial.println(F("ERRO:CARGA_FORA_DA_FAIXA_25_A_50_G"));abortar();return;}
  const char* nomeB=c==0?"B1_CARGA":(c==1?"B2_CARGA":"B3_CARGA");
  if(!fase(1+c*2,nomeB,300000UL))return;
  cargaIni[c]=res[1+c*2].inicial;cargaFim[c]=res[1+c*2].final;
  zeroAntes[c]=(c==0)?res[0].final:zeroDepoisFim[c-1];
  if(!confirmar(F("RETIRE completamente a massa. Mantenha o prato e a plataforma fixos.")))return;
  Serial.println(F("ACOMODACAO_SEM_CARGA:8S"));if(!aguardarMs(8000))return;
  const char* nomeC=c==0?"C1_ZERO":(c==1?"C2_ZERO":"C3_ZERO");
  if(!fase(2+c*2,nomeC,600000UL))return;
  zeroDepoisIni[c]=res[2+c*2].inicial;zeroDepoisFim[c]=res[2+c*2].final;
 }
 relatorio();
}
void loop(){relesOff();}
