# Rastreamento: persistencia de movimento V4 / V10 -> V10.1C

A versão V4 (arquivo original `codigo_automacaoV4.ino`) continha:
- `const int eepromPosStepper = 0;`
- `EEPROM.get(eepromPosStepper, currentPositionSteps);`
- `currentPositionSteps += subir ? 1 : -1;`
- `EEPROM.put(eepromPosStepper, currentPositionSteps);` ao final do movimento.

A versão V10 (`codigo_automacaoV10_HX711.ino`) preservou essa abordagem.

Os testes posteriores de conservacao de massa registraram subida/descida com
`MOVIMENTO:PASSOS_COMANDADOS:14000` e confirmacao manual de chegada. Isso valida
uso desse numero no prototipo, mas nao prova homing automatico em caso de blackout.

A V10.1B usou `HABILITAR_MOVIMENTOS_REAIS = false` e nao persistia passos/angulo.

A V10.1C repoe um registro *novo*, versionado, circular, separado dos primeiros
64 bytes da EEPROM. Registro tem: magic, versao, estado de processo, posicao
comandada, ultimo angulo comandado, etapa, sequencia e CRC16. Atualizacoes:
antes de movimentos, a cada 100 passos de stepper, a cada 5 graus de servo,
nas transicoes de estado e ao encerrar. Em vez de continuar movimento ao
reiniciar, o boot bloqueia inicio quando ha registros interrompidos.

**Nota tecnica:** o firmware V4 que verificamos persistia com certeza a posicao
do motor de passo; nao foi encontrada verificacao independente de EEPROM para o
ultimo angulo do servo. O campo servo foi implementado explicitamente na V10.1C.

A durabilidade do EEPROM e finita. Usar journal circular reduz desgaste por
endereco, mas nao e ideal para ciclos industriais continuos; para produto, avaliar
FRAM externa, sensor de referencia e detecao de alimentacao.
