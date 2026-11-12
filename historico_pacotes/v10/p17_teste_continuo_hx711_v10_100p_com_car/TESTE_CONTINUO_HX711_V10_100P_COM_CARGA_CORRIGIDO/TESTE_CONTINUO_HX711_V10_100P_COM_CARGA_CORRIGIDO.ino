/*
 * TCC Arduino_Grao_CNN - HX711 V10 - ENSAIO COM CARGA FIXA - CORRIGIDO
 *
 * ETAPA 1: prato vazio em X; uma tara inicial (20 conversoes).
 * ETAPA 2: colocar massa fixa de aproximadamente 33,4 g; confirmar C.
 * ETAPA 3: 100 pontos COM A MASSA, 24 conversoes por ponto,
 *          inicio dos pontos a cada 30 s, por 49 min 30 s.
 * ETAPA 4: retirar a massa; confirmar C; verificar o retorno ao zero
 *          com 5 pontos em 2 min. SEM nova tara.
 *
 * Nao altera EEPROM, nao aciona motor/servo nem aquecimento.
 * NAO e firmware de producao: e um ensaio diagnostico independente.
 *
 * SEGURANCA OBRIGATORIA:
 * D6 e D11 sao mantidos em LOW como nos ensaios V10 anteriores.
 * LOW PODE ATIVAR RELES ATIVOS EM NIVEL BAIXO!
 * Somente executar com a ALIMENTACAO DE POTENCIA DO AQUECEDOR
 * FISICAMENTE DESENERGIZADA e a plataforma imovel.
 * A carga total sobre a celula, inclusive prato, deve ser < 1 kg.
 *
 * HX711: DT=A1 SCK=A0, fator de calibracao PROVISORIO -1129.6755.
 * Monitor Serial: 9600 baud, terminador 'Nova linha'.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

const byte PINO_DT = A1;
const byte PINO_SCK = A0;
const byte PINO_DIR = 2;
const byte PINO_STEP = 3;
const byte PINO_RELE = 6;
const byte PINO_SERVO = 9;
const byte PINO_AQUECEDOR = 11;

const float FATOR_CONTAGENS_POR_G = -1129.6755f;
const unsigned long INTERVALO_MS = 30000UL;
const unsigned long TIMEOUT_ADC_MS = 2500UL;
const unsigned long ACOMODACAO_MS = 8000UL;
const byte DESCARTE = 5;
const byte LEITURAS_TARA = 20;
const byte LEITURAS_PONTO = 24;
const byte PONTOS_CARGA = 100;
const byte PONTOS_RETORNO = 5;     // t=0,30,60,90,120 s

HX711 balanca;
long offsetADC = 0;
float pontosCarga[PONTOS_CARGA];
float pontosRetorno[PONTOS_RETORNO];
float somaDP = 0.0f;
float maxDP = 0.0f;
float maiorSalto = 0.0f;
byte indiceSalto = 0;

// Declarado ANTES das funcoes para evitar erros do pre-processador do Arduino IDE.
struct Medicao {
  float peso;
  float dp;
  float amplitude;
  float adcMedio;
};

// Prototipos explicitos: evitam que o Arduino gere prototipos
// para Medicao antes da declaracao da estrutura.
void manterAtuadoresParados();
void configurarSaidas();
char aguardarComando();
bool confirmar(const __FlashStringHelper *mensagem);
bool lerADC(long &adc);
bool medir(Medicao &m);
void mostrarMedicao(const __FlashStringHelper *fase, byte i, byte total, const Medicao &m);
void imprimir(const __FlashStringHelper *nome, float valor);
float mediaPontos(byte inicio, byte fim);
float inclinacaoCarga();
bool coletaComCarga();
bool coletaRetornoZero();
void relatorio();
void setup();
void loop();

void manterAtuadoresParados() {
  digitalWrite(PINO_DIR, LOW);
  digitalWrite(PINO_STEP, LOW);
  digitalWrite(PINO_SERVO, LOW);
  digitalWrite(PINO_RELE, LOW);       // ATENCAO: pode ativar rele ativo-baixo
  digitalWrite(PINO_AQUECEDOR, LOW);  // ATENCAO: pode ativar rele ativo-baixo
}

void configurarSaidas() {
  manterAtuadoresParados();
  pinMode(PINO_DIR, OUTPUT);
  pinMode(PINO_STEP, OUTPUT);
  pinMode(PINO_SERVO, OUTPUT);  // D9 LOW, SEM Servo.h e SEM PWM
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_AQUECEDOR, OUTPUT);
  manterAtuadoresParados();
}

// Aceita 'C' ou 'SAIR' pelo Monitor Serial com 'Nova linha'.
// Esta rotina apenas aguarda comando; nao faz tara nem mede durante a espera.
char aguardarComando() {
  char linha[12];
  byte n = 0;
  while (true) {
    manterAtuadoresParados();
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        linha[n] = '\0';
        if ((n == 1) && (linha[0] == 'C' || linha[0] == 'c')) {
          return 'C';
        }
        if (strcmp(linha, "SAIR") == 0 || strcmp(linha, "sair") == 0) {
          return 'S';
        }
        if (n > 0) Serial.println(F("COMANDO_INVALIDO: envie C ou SAIR"));
        n = 0;
      } else if (n < sizeof(linha)-1) {
        linha[n++] = c;
      } else {
        n = 0;
        Serial.println(F("COMANDO_MUITO_LONGO"));
      }
    }
    delay(20);
  }
}

bool confirmar(const __FlashStringHelper *mensagem) {
  while (Serial.available()) Serial.read();
  Serial.println();
  Serial.println(mensagem);
  Serial.println(F("Digite C + ENVIAR; SAIR cancela (Nova linha)."));
  char c = aguardarComando();
  if (c == 'S') {
    Serial.println(F("TESTE:CANCELADO_PELO_USUARIO"));
    return false;
  }
  Serial.println(F("CONFIRMADO"));
  return true;
}

bool lerADC(long &adc) {
  if (!balanca.wait_ready_timeout(TIMEOUT_ADC_MS)) {
    Serial.println(F("ERRO:HX711_NAO_RESPONDEU"));
    return false;
  }
  adc = balanca.read();
  return true;
}



bool medir(Medicao &m) {
  float mediaG = 0.0f;
  float mediaADC = 0.0f;
  float m2 = 0.0f;
  float minimo = 0.0f;
  float maximo = 0.0f;

  for (byte k = 0; k < LEITURAS_PONTO; ++k) {
    long adc;
    if (!lerADC(adc)) return false;
    float g = ((float)((int64_t)adc-(int64_t)offsetADC)) / FATOR_CONTAGENS_POR_G;
    if (k == 0 || g < minimo) minimo = g;
    if (k == 0 || g > maximo) maximo = g;
    float delta = g - mediaG;
    mediaG += delta / (float)(k + 1);
    m2 += delta * (g - mediaG);
    mediaADC += ((float)adc - mediaADC) / (float)(k + 1);
  }
  m.peso = mediaG;
  m.dp = sqrtf(m2 / (float)(LEITURAS_PONTO - 1));
  m.amplitude = maximo - minimo;
  m.adcMedio = mediaADC;
  return true;
}

void mostrarMedicao(const __FlashStringHelper *fase, byte i, byte total,
                    const Medicao &m) {
  Serial.print(F("PONTO:")); Serial.print(fase);
  Serial.print(F(":")); Serial.print(i + 1);
  Serial.print(F("/")); Serial.print(total);
  Serial.print(F(":TEMPO_S:")); Serial.print((unsigned long)i * (INTERVALO_MS/1000UL));
  Serial.print(F(":PESO_MEDIO_G:")); Serial.print(m.peso, 4);
  Serial.print(F(":RUIDO_DP_G:")); Serial.print(m.dp, 4);
  Serial.print(F(":AMPLITUDE_G:")); Serial.print(m.amplitude, 4);
  Serial.print(F(":ADC_MEDIO:")); Serial.println(m.adcMedio, 1);
}

void imprimir(const __FlashStringHelper *nome, float valor) {
  Serial.print(nome);
  Serial.println(valor, 4);
}

float mediaPontos(byte inicio, byte fim) {
  float soma = 0.0f;
  for (byte i = inicio; i <= fim; ++i) soma += pontosCarga[i];
  return soma / (float)(fim - inicio + 1);
}

// Tendencia de minimos quadrados em g/min para pontos espacados em 0,5 min.
float inclinacaoCarga() {
  const float n = (float)PONTOS_CARGA;
  float somaT = 0.0f, somaY = 0.0f, somaTT = 0.0f, somaTY = 0.0f;
  for (byte i = 0; i < PONTOS_CARGA; ++i) {
    float t = (float)i * 0.5f;
    float y = pontosCarga[i];
    somaT += t;
    somaY += y;
    somaTT += t * t;
    somaTY += t * y;
  }
  float den = n * somaTT - somaT * somaT;
  return den == 0.0f ? 0.0f : (n * somaTY - somaT * somaY) / den;
}

bool coletaComCarga() {
  Serial.println();
  Serial.println(F("FASE:B_CARGA_FIXA_100_PONTOS"));
  unsigned long inicio = millis();
  for (byte i = 0; i < PONTOS_CARGA; ++i) {
    unsigned long alvo = (unsigned long)i * INTERVALO_MS;
    while ((unsigned long)(millis() - inicio) < alvo) {
      manterAtuadoresParados();
      delay(20);
    }
    Medicao m;
    if (!medir(m)) return false;
    pontosCarga[i] = m.peso;
    somaDP += m.dp;
    if (m.dp > maxDP) maxDP = m.dp;
    if (i > 0) {
      float d = fabsf(pontosCarga[i] - pontosCarga[i-1]);
      if (d > maiorSalto) {
        maiorSalto = d;
        indiceSalto = i;
      }
    }
    mostrarMedicao(F("B_CARGA"), i, PONTOS_CARGA, m);
    if (i == 10) Serial.println(F("MARCO:5MIN_SEM_ACAO"));
    if (i == 20) Serial.println(F("MARCO:10MIN_SEM_ACAO"));
    if (i == 40) Serial.println(F("MARCO:20MIN_SEM_ACAO"));
    if (i == 60) Serial.println(F("MARCO:30MIN_SEM_ACAO"));
    if (i == 80) Serial.println(F("MARCO:40MIN_SEM_ACAO"));
  }
  return true;
}

bool coletaRetornoZero() {
  Serial.println();
  Serial.println(F("FASE:C_RETORNO_ZERO_5_PONTOS_SEM_TARA"));
  unsigned long inicio = millis();
  for (byte i = 0; i < PONTOS_RETORNO; ++i) {
    unsigned long alvo = (unsigned long)i * INTERVALO_MS;
    while ((unsigned long)(millis() - inicio) < alvo) {
      manterAtuadoresParados();
      delay(20);
    }
    Medicao m;
    if (!medir(m)) return false;
    pontosRetorno[i] = m.peso;
    mostrarMedicao(F("C_ZERO"), i, PONTOS_RETORNO, m);
  }
  return true;
}

void relatorio() {
  Serial.println(F("============================================"));
  Serial.println(F("RELATORIO_CONTINUO_HX711_V10_100P_COM_CARGA"));
  Serial.println(F("TARA_UNICA:SIM; PINOS:A1/A0; FATOR:-1129.6755"));
  Serial.println(F("PONTOS_CARGA:100"));
  Serial.println(F("INTERVALO_S:30"));
  Serial.println(F("DURACAO_CARGA_S:2970"));
  Serial.println(F("PONTOS_RETORNO_ZERO:5"));
  imprimir(F("PESO_0MIN_G:"), pontosCarga[0]);
  imprimir(F("PESO_5MIN_G:"), pontosCarga[10]);
  imprimir(F("PESO_10MIN_G:"), pontosCarga[20]);
  imprimir(F("PESO_20MIN_G:"), pontosCarga[40]);
  imprimir(F("PESO_30MIN_G:"), pontosCarga[60]);
  imprimir(F("PESO_40MIN_G:"), pontosCarga[80]);
  imprimir(F("PESO_49MIN30S_G:"), pontosCarga[99]);
  imprimir(F("DELTA_0_A_5MIN_G:"), pontosCarga[10] - pontosCarga[0]);
  imprimir(F("DELTA_5_A_10MIN_G:"), pontosCarga[20] - pontosCarga[10]);
  imprimir(F("DELTA_10_A_20MIN_G:"), pontosCarga[40] - pontosCarga[20]);
  imprimir(F("DELTA_20_A_30MIN_G:"), pontosCarga[60] - pontosCarga[40]);
  imprimir(F("DELTA_30_A_40MIN_G:"), pontosCarga[80] - pontosCarga[60]);
  imprimir(F("DELTA_40_A_49MIN30S_G:"), pontosCarga[99] - pontosCarga[80]);
  imprimir(F("DELTA_TOTAL_G:"), pontosCarga[99] - pontosCarga[0]);
  imprimir(F("MEDIA_0_A_5MIN_G:"), mediaPontos(0,10));
  imprimir(F("MEDIA_5_A_10MIN_G:"), mediaPontos(11,20));
  imprimir(F("MEDIA_10_A_20MIN_G:"), mediaPontos(21,40));
  imprimir(F("MEDIA_20_A_30MIN_G:"), mediaPontos(41,60));
  imprimir(F("MEDIA_30_A_40MIN_G:"), mediaPontos(61,80));
  imprimir(F("MEDIA_40_A_49MIN30S_G:"), mediaPontos(81,99));
  imprimir(F("TENDENCIA_GLOBAL_G_POR_MIN:"), inclinacaoCarga());
  imprimir(F("RUIDO_MEDIO_DP_G:"), somaDP / (float)PONTOS_CARGA);
  imprimir(F("MAIOR_RUIDO_DP_G:"), maxDP);
  imprimir(F("MAIOR_SALTO_ENTRE_PONTOS_G:"), maiorSalto);
  Serial.print(F("TEMPO_MAIOR_SALTO_S:"));
  Serial.println((unsigned long)indiceSalto * (INTERVALO_MS/1000UL));
  imprimir(F("ZERO_RETORNO_INICIAL_G:"), pontosRetorno[0]);
  imprimir(F("ZERO_RETORNO_FINAL_G:"), pontosRetorno[PONTOS_RETORNO-1]);
  imprimir(F("VARIACAO_ZERO_RETORNO_G:"), pontosRetorno[PONTOS_RETORNO-1] - pontosRetorno[0]);
  Serial.println(F("SEM_COMPENSACAO; SEM_NOVA_TARA; SEM_EEPROM; SEM_AQUECIMENTO"));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
}

void setup() {
  configurarSaidas();
  Serial.begin(9600);
  balanca.begin(PINO_DT, PINO_SCK);
  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("TESTE:CONTINUO_HX711_V10_100P_COM_CARGA"));
  Serial.println(F("DT=A1 SCK=A0; fator=-1129.6755 PROVISORIO"));
  Serial.println(F("TARA PRATO VAZIO -> CARGA ~33,4g -> 100 PONTOS -> RETORNO ZERO"));
  Serial.println(F("100 pontos/30s; 24 conversoes por ponto; SEM segunda tara"));
  Serial.println(F("ATENCAO: D6/D11 LOW PODE LIGAR RELE ATIVO-BAIXO!"));
  Serial.println(F("AQUECEDOR DEVE ESTAR SEM ALIMENTACAO DE POTENCIA."));
  Serial.println(F("Estabilize Arduino/HX711 energizados por 30min antes de C."));

  if (!confirmar(F("Prato VAZIO e imovel em X; potencia do aquecedor isolada?"))) return;

  Serial.println(F("HX711:DESCARTE_INICIAL:5"));
  for (byte i = 0; i < DESCARTE; ++i) {
    long adc;
    if (!lerADC(adc)) { Serial.println(F("TESTE:ERRO_DESCARTE")); return; }
  }
  Serial.println(F("TARA:REPOUSO_4S"));
  delay(4000);
  int64_t soma = 0;
  for (byte i = 0; i < LEITURAS_TARA; ++i) {
    long adc;
    if (!lerADC(adc)) { Serial.println(F("TESTE:ERRO_TARA")); return; }
    soma += (int64_t)adc;
  }
  offsetADC = (long)(soma / LEITURAS_TARA);
  Serial.print(F("TARA:OK:OFFSET_ADC:")); Serial.println(offsetADC);

  if (!confirmar(F("Coloque a massa FIXA de ~33,4 g no prato e deixe-a imovel."))) return;
  Serial.println(F("ACOMODACAO_CARGA:8S"));
  delay(ACOMODACAO_MS);
  for (byte i = 0; i < DESCARTE; ++i) {
    long adc;
    if (!lerADC(adc)) { Serial.println(F("TESTE:ERRO_ESTABILIZACAO_CARGA")); return; }
  }
  // Confirma se a massa entrou no prato, evitando ensaio vazio acidental.
  Medicao validacao;
  if (!medir(validacao)) { Serial.println(F("TESTE:ERRO_VALIDACAO_CARGA")); return; }
  imprimir(F("VALIDACAO_CARGA_G:"), validacao.peso);
  if (validacao.peso < 20.0f || validacao.peso > 50.0f) {
    Serial.println(F("ERRO:CARGA_FORA_DA_FAIXA_20_A_50_G"));
    Serial.println(F("Confira massa e prato; reinicie com prato vazio para nova tara."));
    return;
  }

  if (!coletaComCarga()) {
    Serial.println(F("TESTE:ERRO_NA_COLETA_COM_CARGA"));
    return;
  }

  if (!confirmar(F("Retire completamente os ~33,4 g, sem tocar na estrutura."))) return;
  Serial.println(F("ACOMODACAO_SEM_CARGA:8S"));
  delay(ACOMODACAO_MS);
  if (!coletaRetornoZero()) {
    Serial.println(F("TESTE:ERRO_NO_RETORNO_ZERO"));
    return;
  }
  relatorio();
}

void loop() {
  manterAtuadoresParados();
  delay(100);
}
