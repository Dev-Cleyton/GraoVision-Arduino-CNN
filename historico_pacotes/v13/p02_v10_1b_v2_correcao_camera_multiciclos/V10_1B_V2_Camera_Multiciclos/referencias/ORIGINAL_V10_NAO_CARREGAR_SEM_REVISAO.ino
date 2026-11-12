
#include <EEPROM.h>
#include <Servo.h>
#include <HX711.h>
#include <math.h>

// === Definições de pinos ===
#define SCK A0
#define DT A1
#define dirPin 2
#define stepPin 3
#define ReleTemp 6
#define servoPin 9
#define heaterPin 11

// === Constantes ===
const int pinNTC = A2;
const float VCC = 5.0;
const float R_FIXED = 10000.0;
const float BETA = 3950.0;
const float T0 = 298.15;
const float R0 = 10000.0;
const unsigned long intervalo = 30000UL;

// === Objetos ===
Servo meuServo;
HX711 escala;

// === Variáveis globais ===
long currentPositionSteps = 0;
// Fator provisório obtido nos três ciclos de calibração do HX711 novo.
// Não recuperar o fator antigo da EEPROM.
const float FATOR_CONTAGENS_POR_G = -1129.6755f;
const byte PESO_AMOSTRAS = 12;
const byte PESO_MAX_TENTATIVAS = 3;
const unsigned long HX_TIMEOUT_MS = 2500UL;
const float LIMITE_AMPLITUDE_G = 0.20f; // Critério provisório; validar com grãos reais.
const unsigned long PAUSA_PESO_MS = 100UL;
float pressaoVaporReal = 10.0;

const int eepromPosStepper = 0;
// Endereço de fator antigo removido: não sobrescrever a calibração nova.

long stepsPerCycle = 14000;
int tempoServo = 3000;
bool valido = true;
bool taraRealizada = false;

// === Máquina de estados ===
enum State {
  IDLE,
  SERVO_DIREITA,
  CAMERA,
  PESAGEM1,
  SERVO_ESQUERDA,
  ATUADOR_SOBE,
  LEITURA_AMBIENTE,
  PESAGEM2,
  ATUADOR_DESCE,
  FIM,
  ERRO
};

State estado = IDLE;

// === Setup ===
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(1000);

  pinMode(ReleTemp, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(heaterPin, OUTPUT);

  digitalWrite(ReleTemp, LOW);
  digitalWrite(heaterPin, LOW);

  meuServo.attach(servoPin);
  meuServo.write(90);

  escala.begin(DT, SCK);

  escala.set_scale(FATOR_CONTAGENS_POR_G);
  // A tara é comandada pelo operador via TARA com o prato metálico vazio.
  // Não executar tara automática ao ligar: a carga pode estar presente.

  EEPROM.get(eepromPosStepper, currentPositionSteps);

  Serial.println(F("READY: Awaiting START command"));
}

// === Loop principal ===
void loop() {
  gerenciarSerial();

  switch (estado) {

    case IDLE:
      break;

    case SERVO_ESQUERDA:
      moverLento(meuServo, 90, 0, 10);
      Serial.println("SERVO:POSICAO_CAMERA");
      delay(5000);//Mudei de 1000 para 5000
      reportarValidation(0, "LOAD", CAMERA);
      return;

    case CAMERA:
      Serial.println("CMD:ACTIVATE_CAMERA");

      valido = aguardarResposta("CAM:SUCESSO", 50000UL);

      if (valido) {
        moverLento(meuServo, 0, 90, 10);
        reportarValidation(0, "GRAO APROVADO", PESAGEM1);
        return;
      }

      // Se não houver aprovação, a amostra não segue
      // para a etapa de pesagem e aquecimento.
      moverLento(meuServo, 0, 90, 10);
      reportarValidation(0, "GRAO REPROVADO", IDLE);
      return;

    case PESAGEM1:
      Serial.println("PESAGEM1");
      {
        float peso;
        if (!medirPesoEstavel(peso)) {
          Serial.println(F("PESO:ERRO"));
          reportarErro(10);
          return;
        }
        enviarPeso("PESO", peso);
        estado = SERVO_DIREITA;
      }
      return;

    case SERVO_DIREITA:
      delay(tempoServo);
      moverLento(meuServo, 90, 180, 10);
      delay(500);
      estado = ATUADOR_SOBE;
      return;

    case ATUADOR_SOBE:
      if (moverAtuador(true, stepsPerCycle)) {
        estado = LEITURA_AMBIENTE;
        return;
      } else {
        reportarErro(4);
      }
      break;

    case LEITURA_AMBIENTE:
      digitalWrite(ReleTemp, HIGH);

      // Envia os dados em mensagens individuais.
      enviarAmbiente();

      // Mantém o intervalo original de 30 segundos.
      delay(intervalo);

      estado = PESAGEM2;
      return;

    case PESAGEM2:
      {
        float peso;
        if (!medirPesoEstavel(peso)) {
          Serial.println(F("PESO2:ERRO"));
          reportarErro(11);
          return;
        }
        enviarPeso("PESO2", peso);
      }

      // Nova leitura ambiental após o intervalo.
      enviarAmbiente();

      digitalWrite(ReleTemp, LOW);

      estado = ATUADOR_DESCE;
      return;

    case ATUADOR_DESCE:
      if (!moverAtuador(false, stepsPerCycle)) {
        reportarErro(4);
        return;
      }
      moverLento(meuServo, 180, 90, 10);
      estado = FIM;
      return;

    case FIM:
      Serial.println("FIM:1");
      estado = IDLE;
      return;

    case ERRO:
      digitalWrite(ReleTemp, LOW);
      digitalWrite(heaterPin, LOW);
      estado = IDLE;
      return;
  }
}

// === Recepção de comandos do Python ===
void gerenciarSerial() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "TARA" && estado == IDLE) {
      // O operador deve deixar somente o recipiente sobre a balança.
      taraRealizada = false;
      if (realizarTaraSegura()) {
        taraRealizada = true;
        Serial.println(F("TARA:OK"));
      } else {
        Serial.println(F("TARA:ERRO"));
      }
    }

    if (cmd == "START" && estado == IDLE) {
      if (taraRealizada) {
        taraRealizada = false; // Exige nova tara para cada ciclo.
        estado = SERVO_ESQUERDA;
      } else {
        Serial.println("ERRO:TARA_NAO_REALIZADA");
      }
    }

    if (cmd.startsWith("TIME_SERVO:")) {
      int segundos = cmd.substring(11).toInt();

      if (segundos > 0) {
        tempoServo = segundos * 1000L;
      }
    }
  }
}

// === Aguarda resposta da câmera ===
bool aguardarResposta(String esperado, unsigned long timeoutMs) {
  unsigned long inicio = millis();

  while (millis() - inicio < timeoutMs) {
    if (Serial.available()) {
      String resposta = Serial.readStringUntil('\n');
      resposta.trim();

      if (resposta.equalsIgnoreCase(esperado) ||
          resposta.equalsIgnoreCase("CAM:OK")) {
        return true;
      }

      if (resposta.equalsIgnoreCase("CAM:RUIM")) {
        return false;
      }

      // Ignora mensagens que não sejam respostas válidas
      // da câmera, sem confundi-las com uma reprovação.
    }
  }

  return false;
}

// === Envio do peso ===
void enviarPeso(String etiqueta, float valor) {
  Serial.print(etiqueta);
  Serial.print(":");
  Serial.println(valor, 2);
}

// === Envio dos dados ambientais ===
void enviarAmbiente() {
  float tempC = lerTemperaturaNTC(pinNTC);
  float umidade = estimarUmidade(tempC, pressaoVaporReal);

  Serial.print("TEMPERATURA:");
  if (isnan(tempC)) {
    Serial.println("ERRO");
  } else {
    Serial.println(tempC, 2);
  }

  Serial.print("UMIDADE:");
  if (isnan(umidade)) {
    Serial.println("ERRO");
  } else {
    Serial.println(umidade, 2);
  }

  delay(1000);
}

// === Leitura do NTC ===
float lerTemperaturaNTC(int pino) {
  int adc = analogRead(pino);

  // Evita divisão por zero e valores inválidos.
  if (adc <= 0 || adc >= 1023) {
    return NAN;
  }

  float Vout = adc * VCC / 1023.0;
  float Rntc = (R_FIXED * Vout) / (VCC - Vout);

  if (Rntc <= 0) {
    return NAN;
  }

  float tempK =
      1.0 / ((1.0 / T0) + (1.0 / BETA) * log(Rntc / R0));

  return tempK - 273.15;
}

// === Estimativa de umidade ===
float estimarUmidade(float tempC, float pressaoVapor) {
  if (isnan(tempC)) {
    return NAN;
  }

  float denominador = 237.3 + tempC;

  if (denominador == 0) {
    return NAN;
  }

  float es = 6.1078 * pow(10, (7.5 * tempC) / denominador);

  if (es <= 0) {
    return NAN;
  }

  float ur = (pressaoVapor / es) * 100.0;

  return constrain(ur, 0, 100);
}

// === Registro de erro ===
void reportarErro(int codigo) {
  Serial.print("ERRO:");
  Serial.println(codigo);

  digitalWrite(ReleTemp, LOW);
  digitalWrite(heaterPin, LOW);

  estado = ERRO;
}

// === Transição de estado ===
void reportarValidation(int codigo, String validation, int estadoValidado) {
  estado = (State)estadoValidado;
}

// === Tara segura: o prato metálico deve estar vazio ===
// O operador confirma o prato vazio antes de enviar TARA.
bool realizarTaraSegura() {
  const byte N = 20;
  int64_t soma = 0;
  for (byte i = 0; i < N; i++) {
    if (!escala.wait_ready_timeout(HX_TIMEOUT_MS)) return false;
    soma += escala.read();
  }
  escala.set_offset((long)(soma / N));
  return true;
}

// === Peso com timeout, repetição e teste de estabilidade ===
// Não mascara valores negativos: eles podem indicar deriva ou tara incorreta.
// Só envia PESO/PESO2 quando a leitura estiver suficientemente estável.
bool medirPesoEstavel(float &peso) {
  for (byte tentativa = 1; tentativa <= PESO_MAX_TENTATIVAS; tentativa++) {
    float soma = 0.0f;
    float minimo = 0.0f;
    float maximo = 0.0f;
    for (byte i = 0; i < PESO_AMOSTRAS; i++) {
      if (!escala.wait_ready_timeout(HX_TIMEOUT_MS)) return false;
      float leitura = escala.get_units(1);
      if (isnan(leitura) || isinf(leitura)) return false;
      if (i == 0 || leitura < minimo) minimo = leitura;
      if (i == 0 || leitura > maximo) maximo = leitura;
      soma += leitura;
      delay(PAUSA_PESO_MS);
    }
    float amplitude = maximo - minimo;
    if (amplitude <= LIMITE_AMPLITUDE_G) {
      peso = soma / PESO_AMOSTRAS;
      return true;
    }
    Serial.print(F("PESO:INSTAVEL:"));
    Serial.println(amplitude, 3);
    delay(500);
  }
  return false;
}

// === Controle do atuador ===
bool moverAtuador(bool subir, long passos) {
  digitalWrite(dirPin, subir ? HIGH : LOW);

  for (long i = 0; i < passos; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(300);

    currentPositionSteps += subir ? 1 : -1;
  }

  EEPROM.put(eepromPosStepper, currentPositionSteps);

  Serial.print("POS_ATUADOR:");
  Serial.println(currentPositionSteps);

  return true;
}

// === Movimento lento do servo ===
void moverLento(Servo &servo, int inicio, int fim, int tempoDelay) {
  if (inicio < fim) {
    for (int pos = inicio; pos <= fim; pos++) {
      servo.write(pos);
      delay(tempoDelay);
    }
  } else {
    for (int pos = inicio; pos >= fim; pos--) {
      servo.write(pos);
      delay(tempoDelay);
    }
  }
}
