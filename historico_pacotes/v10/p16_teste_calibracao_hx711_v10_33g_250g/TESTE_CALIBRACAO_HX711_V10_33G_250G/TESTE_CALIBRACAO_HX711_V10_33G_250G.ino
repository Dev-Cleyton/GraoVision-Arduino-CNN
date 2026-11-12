/*
 * Arduino_Grao_CNN - TESTE_CALIBRACAO_HX711_V10_33G_250G
 * Arduino Uno + HX711 (DT=A1; SCK=A0), 9600 baud.
 *
 * Objetivo: verificar repetibilidade, sensibilidade e retorno ao zero.
 * Cargas: massa ~33,4 g (NAO certificada) e manteiga lacrada de
 * 250 g LIQUIDOS (massa BRUTA inclui a embalagem; NAO e padrao).
 *
 * Sequencia: Z0, M1, Z1, B1, Z2, M2, Z3, B2, Z4.
 * Cada fase: 8 s de acomodacao + 5 descartes + 24 leituras HX711.
 * Cada carga usa a media dos zeros anterior e posterior para uma
 * analise EXPLORATORIA da sensibilidade, sem alterar a tara original.
 *
 * IMPORTANTE: AQUECEDOR SEM ALIMENTACAO DE POTENCIA.
 * O sketch NAO configura nem manipula D2, D3, D6, D9 ou D11.
 * Garanta que nenhum atuador possa ligar acidentalmente.
 * Nao modifica EEPROM, nao altera firmware principal V10.
 * Requer biblioteca HX711 (Bogde) instalada na Arduino IDE.
 * Monitor Serial: 9600 baud; final de linha: Nova linha.
 */

#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include <string.h>

const byte PINO_DT = A1;
const byte PINO_SCK = A0;
const float FATOR_PROVISORIO_CONTAGENS_POR_G = 1134.3f;
const float MASSA_PEQUENA_DECLARADA_G = 33.4f; // valor aproximado, NAO certificado
const float CONTEUDO_LIQUIDO_MANTEIGA_G = 250.0f; // NAO e massa bruta
const byte AMOSTRAS = 24;
const byte DESCARTES = 5;
const unsigned long ACOMODACAO_MS = 8000UL;
const unsigned long TIMEOUT_HX_MS = 2500UL;

HX711 hx;

struct Medicao {
  float mediaADC;
  float dpADC;
  long minimoADC;
  long maximoADC;
};

Medicao dados[9];
const char * const nomes[9] = {
  "Z0_VAZIO_INICIAL", "M1_33G", "Z1_VAZIO",
  "B1_MANTEIGA", "Z2_VAZIO", "M2_33G_REPETICAO",
  "Z3_VAZIO", "B2_MANTEIGA_REPETICAO", "Z4_VAZIO_FINAL"
};

void mostrarOrientacao(byte etapa);
bool esperarConfirmacao();
bool lerUma(long &valor);
bool adquirir(Medicao &m);
void imprimirFase(byte etapa);
void imprimirRelatorio();
float baselineLocal(byte zAntes, byte zDepois);
float deltaLocal(byte carga, byte zAntes, byte zDepois);

bool esperarConfirmacao() {
  char linha[12];
  byte n = 0;
  while (true) {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        linha[n] = '\0';
        if ((n == 1) && ((linha[0] == 'C') || (linha[0] == 'c'))) return true;
        if ((strcmp(linha, "SAIR") == 0) || (strcmp(linha, "sair") == 0)) return false;
        if (n) Serial.println(F("COMANDO_INVALIDO: Digite C ou SAIR"));
        n = 0;
      } else if (n < sizeof(linha) - 1) {
        linha[n++] = c;
      } else {
        n = 0;
        Serial.println(F("COMANDO_MUITO_LONGO"));
      }
    }
    delay(20);
  }
}

bool lerUma(long &valor) {
  if (!hx.wait_ready_timeout(TIMEOUT_HX_MS)) {
    Serial.println(F("ERRO: HX711 sem resposta. Verifique fios e alimentacao."));
    return false;
  }
  valor = hx.read();
  return true;
}

bool adquirir(Medicao &m) {
  long valor;
  for (byte i = 0; i < DESCARTES; i++) {
    if (!lerUma(valor)) return false;
  }
  float media = 0.0f;
  float m2 = 0.0f;
  m.minimoADC = 2147483647L;
  m.maximoADC = -2147483647L;
  for (byte i = 0; i < AMOSTRAS; i++) {
    if (!lerUma(valor)) return false;
    if (valor < m.minimoADC) m.minimoADC = valor;
    if (valor > m.maximoADC) m.maximoADC = valor;
    float x = (float)valor;
    float d = x - media;
    media += d / (float)(i + 1);
    m2 += d * (x - media);
  }
  m.mediaADC = media;
  m.dpADC = sqrtf(m2 / (float)(AMOSTRAS - 1));
  return true;
}

void mostrarOrientacao(byte etapa) {
  Serial.println();
  Serial.print(F("FASE:"));
  Serial.print(etapa + 1);
  Serial.print(F("/9:"));
  Serial.println(nomes[etapa]);
  if ((etapa % 2) == 0) {
    Serial.println(F("ACAO: Retire TODAS as cargas; mantenha apenas o prato original."));
  } else if (etapa == 1 || etapa == 5) {
    Serial.println(F("ACAO: Coloque a MESMA massa aproximada de 33,4 g no centro."));
  } else {
    Serial.println(F("ACAO: Coloque a manteiga LACRADA (250 g de conteudo liquido)."));
    Serial.println(F("NOTA: peso total da embalagem sera maior que 250 g."));
  }
  Serial.println(F("Mantenha prato e estrutura imoveis. Nao apoie a mao."));
  Serial.println(F("Digite C + ENVIAR para medir; SAIR para cancelar."));
}

void imprimirFase(byte etapa) {
  Serial.print(F("PONTO:"));
  Serial.print(nomes[etapa]);
  Serial.print(F(":ADC_MEDIO:"));
  Serial.print(dados[etapa].mediaADC, 2);
  Serial.print(F(":DP_ADC:"));
  Serial.print(dados[etapa].dpADC, 2);
  Serial.print(F(":AMPLITUDE_ADC:"));
  Serial.print(dados[etapa].maximoADC - dados[etapa].minimoADC);
  Serial.print(F(":DP_G_PROVISORIO:"));
  Serial.println(dados[etapa].dpADC / FATOR_PROVISORIO_CONTAGENS_POR_G, 4);
}

float baselineLocal(byte zAntes, byte zDepois) {
  return (dados[zAntes].mediaADC + dados[zDepois].mediaADC) * 0.5f;
}

float deltaLocal(byte carga, byte zAntes, byte zDepois) {
  return dados[carga].mediaADC - baselineLocal(zAntes, zDepois);
}

void imprimirRelatorio() {
  const float d33a = deltaLocal(1, 0, 2);
  const float dBa = deltaLocal(3, 2, 4);
  const float d33b = deltaLocal(5, 4, 6);
  const float dBb = deltaLocal(7, 6, 8);
  const float f33a = d33a / MASSA_PEQUENA_DECLARADA_G;
  const float f33b = d33b / MASSA_PEQUENA_DECLARADA_G;
  const float fatorEstimado = (f33a + f33b) / 2.0f;

  Serial.println();
  Serial.println(F("============================================="));
  Serial.println(F("RELATORIO_CALIBRACAO_HX711_V10_33G_250G"));
  Serial.println(F("TARA_UNICA_PARA_RELATORIO:SIM (Z0)"));
  Serial.println(F("ZERO_LOCAL_INTERPOLADO:SIM (APENAS DIAGNOSTICO)"));
  Serial.println(F("SEM_EEPROM:SIM; SEM_COMPENSACAO_AUTOMATICA:SIM"));
  Serial.print(F("FATOR_PROVISORIO_CONTAGENS_POR_G:"));
  Serial.println(FATOR_PROVISORIO_CONTAGENS_POR_G, 4);
  Serial.print(F("ZERO_INICIAL_ADC:"));
  Serial.println(dados[0].mediaADC, 2);
  Serial.print(F("ZERO_FINAL_ADC:"));
  Serial.println(dados[8].mediaADC, 2);
  Serial.print(F("DESLOCAMENTO_ZERO_ADC:"));
  Serial.println(dados[8].mediaADC - dados[0].mediaADC, 2);
  Serial.print(F("DESLOCAMENTO_ZERO_G_PROVISORIO:"));
  Serial.println((dados[8].mediaADC - dados[0].mediaADC) / FATOR_PROVISORIO_CONTAGENS_POR_G, 4);

  Serial.print(F("DELTA_ADC_33G_1:")); Serial.println(d33a, 2);
  Serial.print(F("DELTA_ADC_33G_2:")); Serial.println(d33b, 2);
  Serial.print(F("REPETIBILIDADE_33G_ADC:")); Serial.println(d33b - d33a, 2);
  Serial.print(F("REPETIBILIDADE_33G_G_PROVISORIO:"));
  Serial.println((d33b - d33a) / FATOR_PROVISORIO_CONTAGENS_POR_G, 4);
  Serial.print(F("PESO_33G_1_G_PROVISORIO:"));
  Serial.println(d33a / FATOR_PROVISORIO_CONTAGENS_POR_G, 4);
  Serial.print(F("PESO_33G_2_G_PROVISORIO:"));
  Serial.println(d33b / FATOR_PROVISORIO_CONTAGENS_POR_G, 4);

  Serial.print(F("DELTA_ADC_MANTEIGA_1:")); Serial.println(dBa, 2);
  Serial.print(F("DELTA_ADC_MANTEIGA_2:")); Serial.println(dBb, 2);
  Serial.print(F("REPETIBILIDADE_MANTEIGA_ADC:")); Serial.println(dBb - dBa, 2);
  Serial.print(F("REPETIBILIDADE_MANTEIGA_G_PROVISORIO:"));
  Serial.println((dBb - dBa) / FATOR_PROVISORIO_CONTAGENS_POR_G, 4);
  Serial.print(F("PESO_BRUTO_MANTEIGA_1_G_PROVISORIO:"));
  Serial.println(dBa / FATOR_PROVISORIO_CONTAGENS_POR_G, 3);
  Serial.print(F("PESO_BRUTO_MANTEIGA_2_G_PROVISORIO:"));
  Serial.println(dBb / FATOR_PROVISORIO_CONTAGENS_POR_G, 3);

  if (fatorEstimado > 0.0f) {
    Serial.print(F("FATOR_ESTIMADO_SE_33G4_FOR_EXATO:"));
    Serial.println(fatorEstimado, 4);
    Serial.print(F("FATOR_DA_PRIMEIRA_33G:")); Serial.println(f33a, 4);
    Serial.print(F("FATOR_DA_SEGUNDA_33G:")); Serial.println(f33b, 4);
    Serial.print(F("DIFERENCA_FATORES_PERCENTUAL:"));
    Serial.println(fabsf(f33a - f33b) * 100.0f / fatorEstimado, 3);
    Serial.print(F("PESO_BRUTO_MANTEIGA_1_COM_FATOR_ESTIMADO:"));
    Serial.println(dBa / fatorEstimado, 3);
    Serial.print(F("PESO_BRUTO_MANTEIGA_2_COM_FATOR_ESTIMADO:"));
    Serial.println(dBb / fatorEstimado, 3);
    Serial.print(F("EMBALAGEM_APARENTE_1_G_SE_33G4_EXATO_E_CONTEUDO_250G:"));
    Serial.println(dBa / fatorEstimado - CONTEUDO_LIQUIDO_MANTEIGA_G, 3);
  } else {
    Serial.println(F("ERRO: sinal invertido ou massa nao detectada; revise ligacoes/fator."));
  }
  Serial.println(F("AVISO: Massa 33,4 g desconhecida; fator calculado NAO certificado."));
  Serial.println(F("AVISO: Manteiga 250 g e PESO LIQUIDO; embalagem possui peso adicional."));
  Serial.println(F("AVISO: Repetibilidade curta NAO comprova ausencia de deriva longa."));
  Serial.println(F("TESTE:FINALIZADO:SUCESSO"));
  Serial.println(F("============================================="));
}

void setup() {
  Serial.begin(9600);
  hx.begin(PINO_DT, PINO_SCK);
  delay(1000);
  Serial.println();
  Serial.println(F("TESTE_CALIBRACAO_HX711_V10_33G_250G"));
  Serial.println(F("HX711 DT=A1 SCK=A0 | 9600 baud"));
  Serial.println(F("9 etapas: VAZIO/33G/VAZIO/MANTEIGA/VAZIO/33G/VAZIO/MANTEIGA/VAZIO"));
  Serial.println(F("24 leituras por etapa, 5 descartes, acomodacao 8 s."));
  Serial.println(F("ATENCAO: AQUECEDOR SEM ALIMENTACAO DE POTENCIA."));
  Serial.println(F("NAO controla D2/D3/D6/D9/D11 nem modifica a V10 principal."));
  Serial.println(F("Ideal: HX711/Arduino energizados por ~30 min antes do teste."));
  Serial.println(F("ATENCAO: 33,4 g NAO CERTIFICADO; manteiga 250 g = PESO LIQUIDO."));

  for (byte i = 0; i < 9; i++) {
    mostrarOrientacao(i);
    if (!esperarConfirmacao()) {
      Serial.println(F("TESTE:CANCELADO"));
      return;
    }
    Serial.println(F("ACOMODACAO:8S"));
    delay(ACOMODACAO_MS);
    if (!adquirir(dados[i])) {
      Serial.println(F("TESTE:INTERROMPIDO_POR_ERRO_HX711"));
      return;
    }
    imprimirFase(i);
  }
  imprimirRelatorio();
}

void loop() {
  // Fim do teste: reinicie o Arduino para uma nova sequencia.
  delay(1000);
}
