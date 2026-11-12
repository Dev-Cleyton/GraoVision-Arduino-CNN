# Protocolo e seguranca V10.1C-R3

## Compatibilidade
`READY:V10.1`, `RECUPERACAO:PENDENTE:...`, `RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL`, `ARMADO:REAL`, `SERVO:CENTRO:OK`, `TARA:OK`, `P1:...`, `CMD:ACTIVATE_CAMERA`, `CAM:SUCESSO` ou `CAM:RUIM`, `P2:...`, `MEDICAO:t:peso:ntc`, `PF:...`, `FIM:1` / `FIM:REPROVADA_CNN`.

Novas mensagens informativas: `STEP:RAMPAS_SUAVES_EEPROM_FORA_DO_MOVIMENTO`, `SERVO:ACOMODADO:PRONTO_PARA_TARA`, `TARA:VERIFICANDO_ESTABILIDADE`, `TARA:INSTAVEL:...`. Elas sao ignoradas pelo parser da R2. Telemetria de motor `PASSOS:SUBIDA:feitos:14000:posicao` / `PASSOS:DESCIDA:feitos:14000:posicao` preservada.

## Protecoes e limitacoes
- Proibido aquecimento: o rele D7 permanece desenergizado por software e o aquecedor deve estar fisicamente desconectado.
- Driver D6 pode desligar em STOP. A vertical deve possuir retencao mecanica independente se houver risco de descida ao cortar energia.
- Falta de energia no meio do curso: EEPROM nao registra os passos intermediarios na R3. O registro fica em `P_EXECUTANDO`, e a recuperacao fisica manual e OBRIGATORIA antes de reiniciar. Nao retomar automaticamente de um valor aproximado.
- `STOP` por serial nao substitui uma parada de emergencia eletrica fisica. E necessario proteger contra esmagamento, colisao e energizacao indevida de reles no reset.
- Servo nao e um sensor de posicao. Os angulos sao comandos de PWM; confirmar mecanicamente.
- NTC A2 nao representa temperatura da camara de secagem; perda de massa nao e umidade absoluta validada.
