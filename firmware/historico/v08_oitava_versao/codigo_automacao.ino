// === Bibliotecas ===
#include <EEPROM.h>
#include <Servo.h>
#include <HX711.h>
//#include <OneWire.h>
//#include <DallasTemperature.h>

// === Definições de Pinos ===
#define SCK A0
#define DT A1
#define dirPin 2
#define stepPin 3
#define ReleTemp 6
#define servoPin 9
#define heaterPin 11

// === constante ===
const int pinNTC = A2; // Temperatura/Umidade o NTC
const float VCC = 5.0;
const float R_FIXED = 10000.0;
const float BETA = 3950.0;
const float T0 = 298.15;
const float R0 = 10000.0;
const long intevalor = 30000;
// === Objetos ===
Servo meuServo;
HX711 escala;

// === Variáveis Globais ===
long currentPositionSteps = 0;
float fatorCalibracao = 420.0;
float pressaoVaporReal = 10.0;// Pressão de vapor real estimada (ex: 10 hPa = ambiente seco)
const int eepromPosStepper = 0;
const int eepromFator = 10;
long stepsPerCycle = 14000;
int tempoServo = 3000; // ms padrão
bool valido = true;
// === Máquina de Estados ===
enum State {
  IDLE, SERVO_DIREITA, CAMERA, PESAGEM1,
  SERVO_ESQUERDA, ATUADOR_SOBE, LEITURA_AMBIENTE,
  PESAGEM2, ATUADOR_DESCE, FIM, ERRO
};
State estado = IDLE;

// === Setup Inicial ===
void setup() {
  Serial.begin(9600);
  pinMode (ReleTemp,OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  digitalWrite(ReleTemp, LOW);
  meuServo.attach(servoPin);
  meuServo.write(90); // Centro

  escala.begin(DT, SCK);
  // Ler EEPROM
  EEPROM.get(eepromFator, fatorCalibracao);
  if (isnan(fatorCalibracao) || fatorCalibracao <= 0 || fatorCalibracao > 10000)
    fatorCalibracao = 420.0;

  escala.set_scale(fatorCalibracao);
  escala.tare();

  EEPROM.get(eepromPosStepper, currentPositionSteps);

  Serial.println("READY: Awaiting START command");
}

// === Loop Principal ===
void loop() {
  gerenciarSerial();

  switch (estado) {
    case IDLE:
      break;

    case SERVO_ESQUERDA:
      moverLento(meuServo, 90, 0, 10);
      delay(1000);
      reportarValidation(000,"LOAD",CAMERA);
      return;
      //estado = CAMERA;
      //break;

    case CAMERA:
      Serial.println("CMD:ACTIVATE_CAMERA");
      valido = aguardarResposta("CAM:SUCESSO", 50000);

      if (valido) {//ACK:CAM
        moverLento(meuServo, 0, 90, 10);
        reportarValidation(000,"GRAO APROVADO",PESAGEM1);
        return;
        //estado = PESAGEM1;
      }

      if(!valido){
        moverLento(meuServo, 0, 90, 10);
        reportarValidation(000,"GRAO REPROVADO",IDLE);
        return;
      }else{
        reportarErro(001);
        moverLento(meuServo, 0, 90, 10);
        estado = IDLE;
        return;
      }
      //break;

    case PESAGEM1:
      Serial.println("PESAGEM1");
      enviarPeso("PESO", mediaPeso());
      estado = SERVO_DIREITA;
      return;
      //break;

    case SERVO_DIREITA:
      delay(tempoServo);
      moverLento(meuServo, 90, 180, 10);
      delay(500);
      estado = ATUADOR_SOBE;
      return;
      //break;

    case ATUADOR_SOBE:
      if (moverAtuador(true, stepsPerCycle)) {
        //meuServo.write(180);
        estado = LEITURA_AMBIENTE;
        return;
      } else {
        reportarErro(004);
      }
      break;

    case LEITURA_AMBIENTE:
      lerTemperaturaNTC(pinNTC);
      digitalWrite(ReleTemp, HIGH);
      enviarAmbiente(); // leitura do NTC + estimativa de umidade
      delay(30000);// Tem Definir o tempo para o processo adequado...
      estado = PESAGEM2;
      return;
      //reak;

    case PESAGEM2:
      enviarPeso("PESO2", mediaPeso());
      lerTemperaturaNTC(pinNTC);
      enviarAmbiente(); // leitura do NTC + estimativa de umidade
      digitalWrite(ReleTemp, LOW);
      estado = ATUADOR_DESCE;
      return;
      //break;

    case ATUADOR_DESCE:
      moverAtuador(false, stepsPerCycle);
      moverLento(meuServo, 180, 90, 10);
      estado = FIM;
      return;
      //break;

    case FIM:
      Serial.println("FIM:1");
      estado = IDLE;
      return;
      //break;

    case ERRO:
      estado = IDLE;
      return;
      //break;
  }
}

// === Funções ===

void gerenciarSerial() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("START")) estado = SERVO_ESQUERDA;//SERVO_DIREITA;
    if (cmd.startsWith("TIME_SERVO:")) tempoServo = cmd.substring(11).toInt() * 1000;
  }
}

bool aguardarResposta(String esperado, int timeoutMs) {
  unsigned long inicio = millis();
  while (millis() - inicio < timeoutMs) {
    if (Serial.available()) {
      String resp = Serial.readStringUntil('\n');
      resp.trim();

      if (resp.equalsIgnoreCase(esperado)) {
        return true;
      } else {
        Serial.print("Resposta inesperada: ");
        Serial.println(resp);
        return false;  // <- nova linha: resposta errada => falso
      }
    }
  }
  Serial.println("Timeout: nenhuma resposta recebida.");
  return false;
}

void enviarPeso(String etiqueta, float valor) {
  Serial.print(etiqueta); Serial.print(":"); Serial.println(valor, 2);
}

void enviarAmbiente() {
  float tempC = lerTemperaturaNTC(pinNTC);
  float umidade = estimarUmidade(tempC, pressaoVaporReal);

  Serial.print("Temperatura: ");
  Serial.print(tempC);
  Serial.print(" °C  | Umidade Estimada: ");
  Serial.print(umidade);
  Serial.println(" %");

  delay(1000);
}

// Função para ler temperatura
float lerTemperaturaNTC(int pino) {
  int adc = analogRead(pino);
  float Vout = adc * VCC / 1023.0;
  float Rntc = (R_FIXED * Vout) / (VCC - Vout);
  float tempK = 1.0 / ((1.0 / T0) + (1.0 / BETA) * log(Rntc / R0));
  return tempK - 273.15;
}

// Estimar umidade com base na temperatura (Tetens)
float estimarUmidade(float tempC, float pressaoVapor) {
  float es = 6.1078 * pow(10, (7.5 * tempC) / (237.3 + tempC)); // pressão saturada
  float ur = (pressaoVapor / es) * 100.0;
  return constrain(ur, 0, 100); // limita entre 0 e 100%
}

void reportarErro(int codigo) {
  Serial.print("ERRO:");
  Serial.println(codigo);
  estado = ERRO;
}

void reportarValidation(int codigo,String validation,int estadoValidado) {
  //Serial.print(validation);
  //Serial.println(codigo);
  //Serial.print(estadoValidado);
  estado = (State)estadoValidado;
  //Serial.print(estado);
}

float mediaPeso() {
  long soma = 0;
  for (int i = 0; i < 100; i++) {
    soma += escala.get_units();
    delay(10);
  }
  return soma / 100.0;
}

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
