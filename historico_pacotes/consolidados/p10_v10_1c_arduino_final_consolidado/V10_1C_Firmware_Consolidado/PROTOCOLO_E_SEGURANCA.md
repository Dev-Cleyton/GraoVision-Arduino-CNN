# Protocolo e salvaguardas — V10.1C

## Contrato antigo preservado
- `READY:V10.1` — sinal de inicializacao/ciclo novo esperado pelo Python V10.1B.
- PC -> Arduino: `TARA`, `START`, `CAM:SUCESSO` (ou `CAM:OK`), `CAM:RUIM`, `CAM:INCONCLUSIVA`, `CAM:ERRO`, `STOP`.
- Arduino -> PC: `TARA:OK`, `P1:<gramas>`, `CMD:ACTIVATE_CAMERA`, `FIM:REPROVADA_CNN`, `FIM:INCONCLUSIVA_CNN`, `P2:<gramas>`, `MEDICAO:<segundos>:<peso_g>:<ntc_C>`, `PF:<gramas>`, `FIM:1`.
- `AQUECEDOR:BLOQUEADO_V10_1`: SEM aquecimento (Python V10.1B mantem relatorio adequado).
- `MOVIMENTO:REAL:SERVO`, `MOVIMENTO:REAL:MOTOR`, `MOVIMENTO:SIMULADO:*`.

## Extensoes V10.1C
- `ARMAR:REAL` -> `ARMADO:REAL` — somente modo real compilado e sem recuperacao pendente; a autorizacao vale um ciclo.
- `ESTADO?` -> `STATUS:ETAPA:<n>:PASSOS:<n>:ANG:<n>:ARMADO:<0|1>` — diagnostico, nao transmite medicao fisica.
- `RECUPERAR:X` -> `RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL` — **apenas** apos realinhamento manual seguro em X inferior e servo centro.
- `RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR` — nao ha `READY:V10.1` ate recuperar.
- `POS_ATUADOR:<n>` — posicao comandada apos subida ou descida.

## Ordem real aprovada
TARA (vazio em X/centro) -> START -> P1 -> servo 0 -> câmera/ROI/CNN ->
se ruim, servo 90 e FIM:REPROVADA_CNN; se boa, servo 90 -> 14000 passos subida ->
servo 180 -> espera -> P2 -> medições locais SEM calor -> PF -> servo 90 ->
14000 passos descida -> FIM:1 -> exige nova tara e novo ARMAR:REAL no ciclo seguinte.

## Salvaguardas implementadas no codigo
- Relês desligados no `setup()`; aquecedor mantido OFF.
- Sem `attach` e sem movimento do servo ao iniciar o Arduino.
- Sem retomar movimento apos interrupcao: EEPROM registra estado interrompido e exige conferência.
- Erros ou `STOP`: parar novos passos, nao reiniciar automaticamente; motor e reles desativados.
- `STOP` via USB nao substitui botao E-STOP por hardware. Perda de alimentacao durante elevacao pode causar queda se o sistema mecanico nao for autotravante.
- `CAM:INCONCLUSIVA` e `CAM:ERRO` nao sao interpretados como grão ruim.
- D6 liga driver apenas se movimentos reais compilados, autorizados e sob controle de relé.

## Semelhancas e limitacoes
- Todos os passos sao contados a partir de uma origem previamente conferida: sem encoder, nao podemos saber se cada pulso virou um passo fisico.
- EEPROM guarda estados comandados. Journal CRC protege contra registros incompletos, mas nao garante posição fisica.
- Nenhum metodo neste firmware calcula teor verdadeiro de umidade. A5 nao configurada; NTC A2 nao mede umidade.
- Sensor NTC A2 fica perto da celula e nao protege camara de secagem.
- Serial 9600; Monitor Serial configurado para Nova linha.

## Confianca e ensaios
A sintaxe C++ foi testada no compilador G++ com stubs de Arduino/HX711/EEPROM/Servo, **nao** compilada para AVR. O firmware deve ser compilado com a biblioteca HX711 Bogde na IDE Arduino Uno e verificado com cargas de potencia desconectadas primeiro.
