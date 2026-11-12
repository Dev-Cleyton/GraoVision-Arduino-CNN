# V10.2.7 R1 — Pausa 1 s, temporizadores e EEPROM

## Ajustes realizados
- `PAUSA_ANTES_TRANSICAO_MS`: 2000 para 1000 ms. Continua cooperativa via `esperarComStop`.
- `TIMEOUT_CNN_MS`: 120 para 240 s, com telemetria da duracao no timeout. Roboflow e webcam podem ter latencia; este prazo e apenas de resposta, nao faz o processo continuar sem CNN.
- Resfriamento: os 300 s contam somente para atingir a temperatura de retorno. A partir da mensagem `RESFRIAMENTO:APTO_PARA_INSPECAO_FISICA`, comeca novo prazo de 180 s para confirmacao humana. A confirmacao recebida move ao estado de retorno apenas se a temperatura continuar <= limiar. Se nao houver confirmacao, continua em falha com a carga termica desligada; NAO desce automaticamente.
- EEPROM journal/CRC, posicao e angulo comandados, movimento 14000, monitoramento, vetores, Python, PWM, motor e STOP: preservados.

## Limites termicos
- Esta compilacao permanece **com aquecedor DESABILITADO** (`AQUECEDOR_PERMITIDO_APOS_INSPECAO = false`). Limites existentes de 50 C corte/35 C retorno foram mantidos para nao transformar ensaio de laboratorio em aprovacao termica. **Nao foi aplicada a proposta 70 C / 50 C** sem qualificacao de NTC, componentes, fonte, corte fisico independente e superficies internas. Se o seu Arduino imprimir `AQUECEDOR:EXIGE_ARMAMENTO_MANUAL_E_CORTE_TERMICO`, ele esta com outro sketch/compilacao — confira antes de qualquer teste.

## EEPROM e retorno automatico
O historico registra a intencao de persistir ultimo passo e angulo para voltar a X. Na implementacao, EEPROM guarda ultima **posicao comandada**, ultimo **angulo comandado**, etapa e situacao do ciclo com CRC, gravando fora do movimento. Quando ocorrer desligamento durante deslocamento, esses dados nao comprovam o local fisico e podem estar desatualizados. SEM HOMING, nao se pode habilitar retorno autonomo universal. Em algumas falhas com eixo PARADO e no extremo logico, a versao preserva `RETORNAR:SUPERVISIONADO` apos verificacao presencial; apos reinicio exige colocar em X e `CONFIRMO-X`.

## Validacao e cautelas
- Executar primeiros testes com alimentacao do aquecedor fisicamente desconectada.
- A compilacao do Arduino nao foi verificada aqui se `arduino-cli` nao esta instalado; compilar ambos `.ino` no Arduino IDE do usuario.
- Testar `CAM:SUCESSO`, CNN lenta, STOP durante pausa, resfriamento que chega ao limiar perto de 300s, confirmacao humana lenta, NTC recuperando temperatura acima do limiar e retorno supervisionado.
- Importante: o Python fica aguardando em `input()` durante a confirmacao do operador; os 180s de firmware vao dar prazo de resposta. Se precisar de inspecao mais longa, o programa deve entrar em estado seguro e exigir uma nova autorizacao em vez de mover sem consentimento.
