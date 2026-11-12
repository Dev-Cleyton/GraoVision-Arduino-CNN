# TCC Grãos — V10.2.2 — Gestão de erros e retorno supervisionado

**Estado: TESTE EXPERIMENTAL. Não validado fisicamente nesta versão.**

Base: V10.2.1, com Servo PWM suspenso durante P2/monitoramento/PF; R3 com rampas; EEPROM journal; CNN/ROI automáticas; P1, P2, PF, compensação e relatórios. **Aquecedor permanentemente bloqueado.**

## Atenção prioritária ao log recebido

`EEPROM:POS_COMANDADA:14000:ANGULO:180` significa **última posição comandada**, não confirmada fisicamente. Se a máquina ainda estiver no alto, **NÃO DIGITE `CONFIRMO-X`**, pois isso não desce o elevador; apenas grava posição zero sem mover nada. A nova opção de retorno supervisionado funciona apenas *na mesma sessão energizada* quando um erro recuperável ocorre com o motor parado. Ela não é oferecida após reinicialização/falta de energia. Para sair de uma recuperação EEPROM pendente em posição física desconhecida, use procedimento mecânico seguro definido pelo responsável pelo equipamento e alinhe fisicamente X antes de `CONFIRMO-X`.

## Instalação

1. Extraia este ZIP numa pasta **separada** da versão anterior.
2. Arquivo Arduino de teste real: `arduino/V10_2_2_Retorno_Supervisionado_Real/V10_2_2_Retorno_Supervisionado_Real.ino`.
3. Arquivo Arduino de simulação: `arduino/V10_2_2_Retorno_Simulacao/V10_2_2_Retorno_Simulacao.ino`.
4. Compile e carregue a versão escolhida via Arduino IDE. O **nome apresentado na serial deve começar com `V10.2.2:`**.
5. Copie o `.env` particular da V10.2.1 para `analise_graos/.env` (NÃO compartilhe a chave Roboflow). Confirme `MOVIMENTOS_REAIS=0` para simulação ou `1` somente com condições de segurança atendidas.
6. Feche Monitor Serial e execute `main.py`. Mantenha o aquecedor fisicamente desconectado.
7. Nunca execute o retorno físico de erro sem **retenção contra queda do elevador**, percurso inspecionado, comando de emergência físico independente e bandeja apoiada quando PWM do servo for suspenso.

## Nova sequência ao ocorrer `ERRO:PESO_MONITOR_INSTAVEL`

```text
[UNO -> PC] ERRO:PESO_MONITOR_INSTAVEL
[UNO -> PC] DIAGNOSTICO_FALHA:ETAPA:9:PASSOS_COMANDADOS:14000:SERVO_ANGULO_COMANDADO:180:MOTOR_EM_MOVIMENTO:0
[UNO -> PC] RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL

[RECUPERACAO] ... VERIFIQUE FISICAMENTE ...
Somente apos conferir, digite RETORNAR para tentar regressar: RETORNAR
[PC -> UNO] RETORNAR:SUPERVISIONADO
[UNO -> PC] RECUPERACAO:RETORNO_INICIADO
[UNO -> PC] SERVO:PWM_REATIVADO:RETORNO
[UNO -> PC] MOVIMENTO:REAL:SERVO
[UNO -> PC] MOVIMENTO:REAL:MOTOR
[UNO -> PC] PASSOS:DESCIDA:0:14000:14000
...
[UNO -> PC] PASSOS:DESCIDA:14000:14000:0
[UNO -> PC] RECUPERACAO:RETORNO_COMANDADO:X_PENDENTE_CONFIRMACAO
[UNO -> PC] RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR
```

A mensagem `RECUPERACAO:RETORNO_COMANDADO...` **não é confirmação de posição física**. Ela indica somente que o Arduino emitiu comandos ao servo e aos 14.000 passos da descida. Antes de iniciar outro ciclo, **conferir fisicamente X e servo ao centro**; o novo ciclo requer `CONFIRMO-X`, armamento `ARMAR` e condições normais.

Se responder qualquer coisa diferente de `RETORNAR`, o equipamento continua em falha; se o elevador estiver acima da base, a V10.2.2 procura manter o driver energizado para não remover a retenção por software, mas isso **não substitui freio mecânico e é ineficaz em falta de energia**.

## Novos dados do relatório

O JSON passa a salvar `codigo_erro_arduino`, `recuperacao_status`, `eventos_recuperacao`, além de P1/P2/PF, offset P2–P1, medições e telemetria de passos. Mesmo se o retorno concluir, o **status final do ensaio é `ERRO_PROCESSO`** e PF não é fabricado.

## Testes recomendados

1. Compilar ambos os sketches no Arduino IDE **sem conectar cargas de potência**.
2. Conferir inicialização, mensagem `V10.2.2`, EEPROM, e `READY` somente com referência confirmada.
3. Em simulação, realizar um erro de leitura e verificar ausência de movimentos reais.
4. Com proteção mecânica e após validação supervisionada, verificar tratamento de `PESO_MONITOR_INSTAVEL` sem calor; primeiro provocar falha em ambiente controlado e sem amostra, se tecnicamente viável.
5. Confirmar que a falha original aparece no JSON e no PowerShell após o retorno.

Este projeto não fornece homing físico, encoder, garantia de sustentação mecânica, temperatura na câmara, corte térmico independente nem implementação de aquecimento.
