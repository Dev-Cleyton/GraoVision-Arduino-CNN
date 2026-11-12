# V10.2.1 — Suspensao PWM do servo durante P2/monitoramento/PF

## O que mudou

- Apos `moverServo(180)` e sua acomodacao, o Arduino chama `servoPrato.detach()` para interromper APENAS os pulsos de controle de D9.
- Aguarda os 15 s de estabilizacao ja existentes antes de P2.
- Durante P2, todas as MEDICAO e PF, o PWM continua suspenso. O firmware verifica que o servo nao foi reanexado acidentalmente durante essas etapas.
- Antes de retornar 180 -> 90, reanexa com `servoPrato.write(anguloAtual)` seguido de `servoPrato.attach(PIN_SERVO)` e executa o movimento gradual preexistente.
- Console serial: `SERVO:PWM_SUSPENSO:SECAGEM`, depois `SERVO:PWM_REATIVADO:RETORNO`.
- O Python guarda esses eventos no `*_relatorio.json` e preserva a compensacao P2-P1 mesmo quando o processo falha antes de PF.
- EEPROM, ROI automatica, CNN, motor de passo com rampas, contagem a cada 500 pulsos e aquecedor bloqueado foram mantidos.

## SEGURANCA — ler antes de executar

`detach()` **NAO desliga os 5 V do servo**; interrompe somente os pulsos PWM de controle. O SERVO pode perder torque. So use o firmware REAL se a bandeja estiver MECANICAMENTE apoiada / travada na posicao de secagem, sem depender do torque do servo. Caso contrario use o sketch de SIMULACAO e implemente retencao mecanica antes de qualquer ensaio fisico.

MANTER o aquecedor fisicamente desconectado. O NTC A2 mede a area da celula de carga e nao supervisiona a camara. Falha/STOP no topo pode cortar rele de driver D6: eixo vertical precisa ser mecanicamente antqueda e possui parada de emergencia eletrica independente. Nao confunda pulsos comandados com posicao real medida.

## Instalar

1. Extraia em uma nova pasta, preserve sua V10.2 anterior.
2. No Arduino IDE, carregue exclusivamente `arduino/V10_2_1_Servo_PWM_Movimento_Real_Supervisionado/V10_2_1_Servo_PWM_Movimento_Real_Supervisionado.ino` para movimento supervisionado, OU use `arduino/V10_2_1_Servo_PWM_Simulacao_Segura` para testar sem movimento.
3. Compile e faca Upload, verifique que a inicializacao traz `V10.2.1:REAL_SUPERVISIONADO_PWM_SUSPENSO_SEM_AQUECIMENTO` e `SERVO:PWM_OFF_DURANTE_PESAGEM_HABILITADO`.
4. Copie seu `.env` privado anterior para `analise_graos/.env` (nao compartilhe a API key). Mantenha `MOVIMENTOS_REAIS=1` somente se as protecoes mecanicas estiverem instaladas e testadas.
5. Execute `& "C:/Program Files/Python313/python.exe" ".\main.py"` no PowerShell dentro de `analise_graos`.
6. Confirme fisicamente posicao X, percurso e apoio mecanico SEM torque antes de ARMAR. Em caso de RECUPERACAO:PENDENTE, somente confirmar X quando a base realmente estiver embaixo e servo centralizado.
7. Observe `SERVO:PWM_SUSPENSO:SECAGEM` ANTES de P2, e `SERVO:PWM_REATIVADO:RETORNO` APOS PF, antes de retorno. Compare PESO:INSTAVEL e series em repeticoes com a V10.2.

**Nao ha sensor de posicao angular ou homing.** Reanexar pode provocar pequeno tranco se o servo cedeu durante a suspensao do PWM. Pare o ensaio e nao habilite esta versao se houver movimento espontaneo ou queda quando PWM for suspenso.

## Como comparar

Compare 3+ ciclos sem aquecimento com amostras semelhantes, mesma carga e condicoes mecanicas: numero de ocorrencias `PESO:INSTAVEL`, variacao P2-PF, repetibilidade. Relatorio interrompido mantem P1 e P2, offset e PF nulo. A estabilidade deve ser validada experimentalmente.

## Validacao deste pacote

Os testes Python e a checagem C++ com stubs verificam regressoes e sintaxe; NAO equivalem a compilacao para ATmega328P nem comprovam comportamento fisico.
