# TCC Graos — V10.1C: Firmware Arduino Consolidado

**Escopo:** prototipo de bancada supervisionado. **NAO e sistema autonomo de producao com aquecimento.**

## Conteudo
- `arduino/V10_1C_Firmware_Consolidado/V10_1C_Firmware_Consolidado.ino`: firmware Arduino Uno para HX711, NTC, servo, stepper, reles, EEPROM, protocolo Python.
- `analise_graos/`: projeto Python V10.1B V2, com ROI automatica e correcao da webcam entre ciclos, acrescido de armamento explicito de movimentos da V10.1C.
- `HISTORICO_EEPROM.md`: o que veio das versoes originais e por que a recuperacao e supervisionada.
- `PROTOCOLO_E_SEGURANCA.md`: mensagens seriais, configuracao e limites de seguranca.

## Mapeamento confirmado
| Parte | Arduino |
|---|---|
| HX711 DT / SCK | A1 / A0 |
| NTC local | A2 |
| Driver TB6600 DIR / STEP | D2 / D3 |
| Servo | D9 |
| Rele aquecedor IN1 (LOW liga) | D7 — **NUNCA ENERGIZADO** |
| Rele driver IN2 (LOW liga) | D6 |
| Rele IN3 / IN4 (reservas) | D8 / D4 |

## Modos de funcionamento
O sketch vem com `PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO = false`.

**Modo de compatibilidade (padrao):** sobe/desce e servo simulados; a V10.1B V2 continua funcionando sem alterar o ArduinoHandler. Com `MOVIMENTOS_REAIS=0` no `.env`, a interface Python segue as telas anteriores.

**Modo real supervisionado:** requer instalar e validar uma parada de emergencia fisica, mecanismo contra queda, curso e zero verificados, aquecedor fisicamente desconectado. Depois, na copia do sketch, alterar `PERMITIR_MOVIMENTO_REAL_NESTA_COMPILACAO` para `true`, compilar e carregar no Arduino, e definir `MOVIMENTOS_REAIS=1` no `.env` do Python. O operador precisa conferir X inferior + centro e escrever `ARMAR` na interface a cada ciclo. O Python envia `ARMAR:REAL` e o Arduino confirma `ARMADO:REAL`. Em nenhuma hipotese isso torna o aquecedor ativo.

**Driver pelo rele D6:** no sketch a opcao `DRIVER_CONTROLADO_PELO_RELE_D6 = true` corresponde ao mapeamento IN2=D6. Ao habilitar movimento real, verifique a alimentacao e especialmente se a mecanica continua segura quando o driver perde energia. O driver fica ligado entre subida e descida para permitir retencao, mas o programa nao substitui um freio mecanico.

## Como carregar no Arduino IDE
1. Instalar as bibliotecas `HX711` (Bogde) e `Servo` (inclusa na plataforma AVR); `EEPROM` faz parte do core Arduino.
2. Abrir exatamente `V10_1C_Firmware_Consolidado.ino` na pasta de mesmo nome.
3. Placa **Arduino Uno**, porta COM que corresponda ao dispositivo, taxa serial 9600.
4. Compilar e carregar com **cargas de potencia desconectadas**.
5. Extrair o Python desta pasta para uma pasta nova; copiar seu `.env` secreto para `analise_graos/.env` e manter `MOVIMENTOS_REAIS=0` no primeiro teste de integracao.
6. Abrir o PowerShell **dentro** de `analise_graos` e executar:

```powershell
& "C:/Program Files/Python313/python.exe" ".\main.py"
```

7. No primeiro ciclo, verificar `READY:V10.1`, `TARA:OK`, `P1`, `CMD:ACTIVATE_CAMERA`, `CAM:SUCESSO` ou `CAM:RUIM`, `P2`, `MEDICAO`, `PF`, `FIM`.

## EEPROM: primeiro uso, desligamento e recuperacao
A V4/V10 gravava a posicao em endereco 0. Esta versao **nao apaga os 64 primeiros bytes**; usa um jornal a partir de endereco 64, com CRC16, sequencia e rotacao entre entradas.

Em modo real, apos a **primeira gravacao** do sketch ou apos interrupcao de um movimento, e esperado:

```
RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR
```

Nao e um defeito: o firmware nao conhece sua posicao real. **Sem movimentar automaticamente pela USB**, desligar a potencia dos atuadores e aquecedor, retirar a amostra, levar o mecanismo a X inferior e servo centro, conferir fisicamente, ligar o Arduino, abrir o Monitor Serial (9600; fim de linha `Nova linha`) e enviar:

```
RECUPERAR:X
```

O firmware grava a referencia confirmada e responde `RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL` e `READY:V10.1`. Fechar o Monitor Serial antes de iniciar o Python. Se o equipamento nao puder ser realinhado de forma segura, **nao enviar `RECUPERAR:X`**; interrompa o teste.

Apos falta de energia **nao existe retomada automatica do meio do movimento**: a EEPROM preserva a ultima posicao *comandada* para diagnostico, mas um passo pode ser perdido ou o servo pode mover-se sem energia. A retomada segura e reiniciar o ciclo somente apos realinhamento confirmado.

## O que falta para uso realmente produtivo com secagem
- Sensor de temperatura na **camara de secagem** e validacao de sua leitura e limites, independente do NTC A2 instalado perto da celula.
- Corte termico independente do Arduino, e protecao eletrica do aquecedor/rele inclusive durante boot/reset.
- Fins de curso/homing ou verificacao de posicao fisica com seguranca, parada de emergencia e mecanismo anticaida.
- Validacao gravimetrica do teor de umidade; NTC e variacao de massa em 2 minutos sem calor **nao determinam umidade**.
- Teste em condicoes fisicas reais e aprovacao da logica de recuperacao.

**Testes executados aqui:** verificacao sintatica C++ com cabecalhos simulados do Arduino, compilacao sintatica de Python e 21 testes unitarios offline. A compilacao com toolchain AVR e o teste na placa Arduino fisica **ainda devem ser realizados por voce**.
