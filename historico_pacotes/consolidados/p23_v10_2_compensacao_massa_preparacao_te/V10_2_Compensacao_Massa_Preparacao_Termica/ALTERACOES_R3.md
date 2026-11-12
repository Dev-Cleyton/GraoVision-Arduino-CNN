# Mudancas objetivas R2 → R3

| Item | R2 | R3 |
|---|---|---|
| Centro antes da tara | `SERVO:CENTRO:OK` apos 500 ms | 2,5 s de acomodacao + confirmacao presencial ENTER + teste HX711 estavel |
| Tara | 20 leituras sem amplitude | ate 3 janelas de 20 leituras, amplitude <=0,20 g |
| Servo 90°→180° | 1°/12ms + EEPROM/5° | 1°/24ms, sem EEPROM durante giro |
| Stepper 14.000 | periodo 600 µs/passo + EEPROM/100 passos | periodo 1800→750→1800 µs com rampas de 1200 passos, EEPROM somente antes e depois |
| Telemetria | a cada 500 passos | preservada |
| Espera antes de P2 | 8 s | 15 s |
| Aquecedor | bloqueado | bloqueado |

**Compromisso importante:** R3 troca o registro frequente de posicao intermediaria na EEPROM por deslocamento sem bloqueios. Em uma falta de energia DURANTE O MOVIMENTO nao se sabe exatamente onde a base ficou: o sistema deve exigir realinhamento manual, nunca tentar retomar a partir do valor gravado. Para futura retomada automatica precisa de homing/fim de curso ou encoder e seguranca adequada.
