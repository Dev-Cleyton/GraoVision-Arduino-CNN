/*
  TESTE_MODULO_RELE.ino
  Arduino Uno + Screw Shield 1.0 + modulo 5V / 4 canais

  LIGACOES INFORMADAS:
  IN1 -> D7
  IN2 -> D6
  IN3 -> D5
  IN4 -> D4
  VCC -> 5V apropriados ao modulo
  GND -> GND comum ao Arduino

  SEGURANCA:
  - DESLIGUE E DESCONECTE fisicamente as cargas dos contatos
    dos reles antes de executar este programa.
  - Nao conecte aquecedor nem alimentacao do driver aos contatos
    durante este ensaio.
  - Teste SOMENTE o lado de controle 5V dos reles.
  - Pode haver acionamento momentaneo durante reset/boot,
    fora do controle do programa.
  - Confirme a polaridade do modulo. Padrao aqui: ATIVO EM LOW.
  - Nao controla HX711, NTC, servo, motor ou D11.

  COMANDOS (Monitor Serial 9600 baud, Nova linha):
  C = liberar testes (somente apos desconectar as cargas)
  1/2/3/4 = pulsar um rele por 2 segundos
  T = testar os 4 reles, UM DE CADA VEZ, por 2 segundos
  0 = cancelar teste e comandar todos DESLIGADOS
  S = mostrar status e comandos
  X = cancelar e bloquear testes ate novo C
*/

#include <Arduino.h>

const uint8_t PINOS_RELE[4] = {7, 6, 5, 4};

// Em modulos de rele ativos em nivel BAIXO: LOW = LIGA.
// Caso o seu modulo seja comprovadamente ativo em nivel ALTO,
// mude SOMENTE esta constante para HIGH, ainda sem cargas ligadas.
const uint8_t NIVEL_ATIVO = LOW;
const uint8_t NIVEL_INATIVO = (NIVEL_ATIVO == LOW) ? HIGH : LOW;

const unsigned long DURACAO_PULSO_MS = 2000UL;
const unsigned long INTERVALO_AUTOMATICO_MS = 800UL;

enum EstadoTeste : uint8_t { PARADO, PULSANDO, INTERVALO };
EstadoTeste estado = PARADO;
bool autorizado = false;
bool sequenciaAutomatica = false;
uint8_t releAtual = 0;
unsigned long instanteEstado = 0;

void comandarTodosDesligados() {
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(PINOS_RELE[i], NIVEL_INATIVO);
  }
}

void cancelarTeste() {
  comandarTodosDesligados();
  estado = PARADO;
  sequenciaAutomatica = false;
}

void mostrarStatus() {
  Serial.println(F("------------------------------------------"));
  Serial.println(F("TESTE_MODULO_RELE - STATUS"));
  Serial.println(F("IN1=D7 | IN2=D6 | IN3=D5 | IN4=D4"));
  Serial.print(F("POLARIDADE: "));
  Serial.println(NIVEL_ATIVO == LOW ? F("LOW=LIGA / HIGH=DESLIGA") : F("HIGH=LIGA / LOW=DESLIGA"));
  Serial.print(F("TESTES: "));
  Serial.println(autorizado ? F("LIBERADOS") : F("BLOQUEADOS"));
  Serial.print(F("ESTADO COMANDADO: "));
  if (estado == PULSANDO) {
    Serial.print(F("RELE "));
    Serial.print(releAtual + 1);
    Serial.println(F(" LIGADO (PULSO)"));
  } else {
    Serial.println(F("TODOS DESLIGADOS"));
  }
  Serial.println(F("C=LIBERAR | 1..4=TESTE INDIVIDUAL | T=SEQUENCIA"));
  Serial.println(F("0=DESLIGAR/CANCELAR | S=STATUS | X=BLOQUEAR"));
  Serial.println(F("------------------------------------------"));
}

void iniciarPulso(uint8_t indice, bool automatica) {
  comandarTodosDesligados(); // nunca ligar dois canais juntos
  releAtual = indice;
  sequenciaAutomatica = automatica;
  digitalWrite(PINOS_RELE[releAtual], NIVEL_ATIVO);
  instanteEstado = millis();
  estado = PULSANDO;

  Serial.print(F("RELE:"));
  Serial.print(releAtual + 1);
  Serial.print(F(":INICIO_PULSO:PIN_D"));
  Serial.print(PINOS_RELE[releAtual]);
  Serial.println(F(":DURACAO_2S"));
  Serial.println(F("OBSERVE LED E CLIQUE DO RELE."));
}

void tratarComando(char comando) {
  if (comando >= 'a' && comando <= 'z') comando -= ('a' - 'A');
  if (comando == '\r' || comando == '\n' || comando == ' ' || comando == '\t') return;

  if (comando == '0') {
    cancelarTeste();
    Serial.println(F("COMANDO:0:TODOS_DESLIGADOS"));
    return;
  }
  if (comando == 'X') {
    cancelarTeste();
    autorizado = false;
    Serial.println(F("COMANDO:X:TESTES_BLOQUEADOS"));
    return;
  }
  if (comando == 'S') {
    mostrarStatus();
    return;
  }
  if (comando == 'C') {
    if (estado != PARADO) {
      Serial.println(F("AVISO:TESTE_EM_ANDAMENTO:USE_0_PARA_PARAR"));
      return;
    }
    autorizado = true;
    comandarTodosDesligados();
    Serial.println(F("CONFIRMADO:SAIDAS_DE_POTENCIA_DESCONECTADAS_POR_USUARIO"));
    Serial.println(F("TESTES_LIBERADOS:ENVIE_1_2_3_4_OU_T"));
    return;
  }
  if (!autorizado) {
    Serial.println(F("BLOQUEADO: DESCONECTE TODAS AS CARGAS DOS CONTATOS"));
    Serial.println(F("APOS CONFERIR, DIGITE C + ENVIAR (Nova linha)."));
    return;
  }
  if (estado != PARADO) {
    Serial.println(F("TESTE_EM_ANDAMENTO:AGUARDE_OU_ENVIE_0"));
    return;
  }
  if (comando >= '1' && comando <= '4') {
    iniciarPulso((uint8_t)(comando - '1'), false);
  } else if (comando == 'T') {
    Serial.println(F("TESTE_AUTOMATICO:4_CANAIS_SEQUENCIAIS"));
    iniciarPulso(0, true);
  } else {
    Serial.println(F("COMANDO_INVALIDO:ENVIE_S_PARA_VER_COMANDOS"));
  }
}

void atualizarTeste() {
  if (estado == PULSANDO && millis() - instanteEstado >= DURACAO_PULSO_MS) {
    comandarTodosDesligados();
    Serial.print(F("RELE:"));
    Serial.print(releAtual + 1);
    Serial.println(F(":FIM_PULSO:DESLIGADO"));

    if (sequenciaAutomatica && releAtual < 3) {
      instanteEstado = millis();
      estado = INTERVALO;
    } else {
      estado = PARADO;
      sequenciaAutomatica = false;
      Serial.println(F("TESTE_CONCLUIDO:TODOS_DESLIGADOS"));
    }
  }

  if (estado == INTERVALO && millis() - instanteEstado >= INTERVALO_AUTOMATICO_MS) {
    iniciarPulso(releAtual + 1, true);
  }
}

void setup() {
  // Primeiro prepara o latch no nivel inativo; depois habilita saida.
  // Isso reduz pulsos espurios apos o firmware assumir o controle,
  // mas NAO elimina a incerteza durante boot/reset do Arduino.
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(PINOS_RELE[i], NIVEL_INATIVO);
  }
  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(PINOS_RELE[i], OUTPUT);
  }
  comandarTodosDesligados();

  Serial.begin(9600);
  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F("TESTE_MODULO_RELE - V1"));
  Serial.println(F("ARDUINO UNO + SCREW SHIELD + 4 RELES 5V"));
  Serial.println(F("IN1=D7 | IN2=D6 | IN3=D5 | IN4=D4"));
  Serial.println(F("TODOS OS RELES COMANDADOS DESLIGADOS"));
  Serial.println(F("ATENCAO: NAO TESTAR COM AQUECEDOR/DRIVER LIGADOS"));
  Serial.println(F("ATENCAO: CONTATOS SEM NENHUMA CARGA ENERGIZADA"));
  Serial.println(F("POLARIDADE PREVISTA: LOW=LIGA"));
  Serial.println(F("=========================================="));
  Serial.println(F("DESCONECTE FISICAMENTE AS CARGAS DOS CONTATOS."));
  Serial.println(F("CONFIRA E DIGITE C + ENVIAR (Nova linha)."));
}

void loop() {
  while (Serial.available() > 0) {
    tratarComando((char)Serial.read());
  }
  atualizarTeste();
}
