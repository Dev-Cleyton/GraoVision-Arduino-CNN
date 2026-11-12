# Protocolo de recuperação V10.2.2

1. O Arduino detecta falha e interrompe o processo; a falha original é emitida como `ERRO:CODIGO`.
2. O Arduino emite `DIAGNOSTICO_FALHA:ETAPA:n:PASSOS_COMANDADOS:n:SERVO_ANGULO_COMANDADO:n:MOTOR_EM_MOVIMENTO:0/1`.
3. Apenas quando o retorno é elegível em **mesma sessão**, motor parado e posições lógicas válidas, emite `RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL`.
4. Python exibe alertas e exige `RETORNAR` presencial.
5. Se confirmado, PC envia `RETORNAR:SUPERVISIONADO`.
6. O Arduino responde `RECUPERACAO:RETORNO_INICIADO`, reativa PWM do servo quando necessário, envia servo ao centro e retorna a base com rampa do stepper.
7. Em sucesso lógico, emite `RECUPERACAO:RETORNO_COMANDADO:X_PENDENTE_CONFIRMACAO` e mantém EEPROM com **falha pendente**. Nunca emite `FIM:1` de ciclo normal.
8. Próximo ciclo: operador confere fisicamente base X e centro do servo, então `CONFIRMO-X` autoriza `RECUPERAR:X` apenas se a máquina estiver efetivamente alinhada.

Se houver `STOP`, EEPROM inválida, falta de energia, movimento interrompido, erro mecânico ou posição real não verificável, o retorno não é oferecido; é necessária intervenção manual segura. Nenhum comando compensa a ausência de sensores de posição e proteção contra queda.
