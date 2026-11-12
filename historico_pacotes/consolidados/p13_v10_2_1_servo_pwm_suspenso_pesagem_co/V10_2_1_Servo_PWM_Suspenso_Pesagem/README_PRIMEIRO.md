# PRIMEIRO: V10.2.1 — PWM de servo suspenso durante pesagem

Leia `LEIA_PRIMEIRO_V10_2_1.md` para escolher e carregar o sketch certo. **NAO carregue um firmware antigo R3 com o Python novo**, embora o protocolo serial seja compativel.

## Sketches

- `arduino/V10_2_1_Servo_PWM_Movimento_Real_Supervisionado/V10_2_1_Servo_PWM_Movimento_Real_Supervisionado.ino` — apenas com retencao mecanica da bandeja e do elevador, emergencia eletrica e autorizacao `ARMAR`.
- `arduino/V10_2_1_Servo_PWM_Simulacao_Segura/V10_2_1_Servo_PWM_Simulacao_Segura.ino` — sem movimento fisico.

## Mudancas

- `Servo.detach()` depois do giro de 180 graus e antes de P2, durante toda serie e ate PF.
- `Servo.attach()` ao iniciar o retorno do servo para 90 graus, usando antes o angulo comandado armazenado.
- O Arduino nao desliga 5V de D9. O pulso de comando e desativado.
- `*_relatorio.json` registra eventos PWM; offset P2-P1 fica salvo mesmo apos erro com PF ausente.
- Mantidos 14000 passos, rampas, EEPROM sem gravacao durante movimentos, ROI automatica e CNN.
- Aquecedor continua fisicamente DESCONECTADO e BLOQUEADO no firmware.

## Limite fundamental

O servo pode perder torque sem PWM: nao testar com bandeja suspensa apenas pelo servo. No erro de peso o equipamento pode permanecer no topo; a logica antiga corta alimentacao do driver em STOP/ERRO. Sem freio mecanico antiqueda e parada de emergencia fisica, utilizar a simulacao. Nao ha protecao termica da camara.

Validacao offline apenas: Python e sintaxe C++ com stubs; necessario compilacao/upload e ensaio supervisionado.
