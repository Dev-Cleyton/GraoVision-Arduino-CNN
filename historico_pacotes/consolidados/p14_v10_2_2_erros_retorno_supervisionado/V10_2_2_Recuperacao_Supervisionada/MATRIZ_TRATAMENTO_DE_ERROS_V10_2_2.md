# Matriz de tratamento de falhas — V10.2.2

| Evento | Decisão Arduino | Retorno | Relatório |
|---|---|---|---|
| `PESO:INSTAVEL:...` durante uma tentativa | Aviso; aguarda tentativas previstas | Não aplicável | Histórico de leituras ainda válido |
| `ERRO:PESO_MONITOR_INSTAVEL` | Encerra medição, mantém aquecedor OFF; se alto, procura manter driver D6 energizado | **Somente supervisionado**, na mesma sessão, motor parado, curso lógico em extremo | Erro original, P1/P2, séries até falha, retorno e telemetria |
| `ERRO:P2_INVALIDO_OU_INSTAVEL` | Falha persistida; motor parado no alto | Somente supervisionado nas mesmas condições | P1 e erro, sem P2/PF inválidos |
| `ERRO:P1_INVALIDO_OU_INSTAVEL` | Falha com base lógica em X | Somente supervisionado (servo central, se necessário) | P1 não inventado |
| `ERRO:CNN_TIMEOUT` | Falha enquanto câmera aguarda | Somente supervisionado na base | Erro, imagens existentes |
| `ERRO:FALHA_CAMERA_CNN` | Recebe `CAM:ERRO` (não considera grãos reprovados) | Somente supervisionado na base | Motivo original Python preservado, erro Arduino registrado |
| `ERRO:TARA_INSTAVEL`, `ERRO:TARA_HX711_TIMEOUT` | Bloqueia, persistência P_FALHOU | **Não** tenta mover; revisão da célula | Erro e estado |
| `ERRO:EEPROM_FALHA`, `ERRO:EEPROM_FALHA_NO_RETORNO` | Bloqueia movimento, estado incerto | **Não** | Erro; revisão física obrigatória |
| `ERRO:POSICAO_LOGICA_MOTOR_INVALIDA`, `ERRO:POSICAO_FINAL_INCONSISTENTE` | Bloqueia motor | **Não** | Erro e última posição comandada |
| `ERRO:SERVO_NAO_AUTORIZADO`, `ERRO:MOTOR_NAO_AUTORIZADO` | Bloqueia | **Não** | Erro; revisar armamento |
| `ERRO:PWM_SERVO_ATIVO_DURANTE_PESAGEM` ou `...REATIVADO_DURANTE_MONITOR` | Bloqueia por condição não prevista | **Não** | Erro de integridade do PWM |
| `ERRO:STOP_OPERADOR`, emergência física, colisão ou interrupção de movimento | Parada imediata do fluxo lógico; não inicia retorno | **Não**; avaliar mecanicamente | Causa; não usar retorno automático |
| Falta de energia, reinicialização com P_EXECUTANDO/P_FALHOU, EEPROM com 14000/180 | Não transmite READY; exige referência | **Não por software após reboot** | Journal indica **última posição comandada**, não física |
| Rejeições de protocolo `ERRO:RECUPERACAO_PENDENTE`, `ERRO:REAL_NAO_ARMADO`, etc. | Recusa o comando | **Não** | Mensagem de autorização/estado |
| `MEDICAO:...:ERRO` | NTC local indisponível, sem calor nesta versão | Não é gatilho de retorno mecânico | Temperatura `null`; não substituir por valor inventado |
| Desconexão serial Python ↔ Arduino | Sem garantia de sincronização; timeout da CNN se aplicável | **Não** sem nova verificação física | Exceção de comunicação |

## Semântica dos estados

- **RETORNO DISPONÍVEL**: o código constatou condições *lógicas* permitidas; não detecta carga física, obstáculo, posição real ou torque.
- **RETORNO COMANDADO**: pulsos e comandos de ângulo foram enviados; não certifica chegada.
- **RECUPERACAO:PENDENTE**: mesmo após retorno comandado, a execução de novo ciclo depende de conferência física X + centro e confirmação.
- **FIM:1**: exclusivo de ciclo normal, jamais emitido após retorno de falha.
- **ERRO_PROCESSO**: status definitivo do ciclo interrompido, inclusive se o retorno comandado for concluído.

## Proteções externas necessárias

O relé D6 pode precisar permanecer ligado para sustentar o conjunto em Y. Cortar sua alimentação no alto pode provocar queda mecânica. Uma parada de emergência elétrica não pode depender do Arduino, e um equipamento vertical deve ter retenção mecânica contra queda independente da alimentação, além de fins de curso/homing para automação sem operador. Aquecedor D7 continua OFF neste firmware; NTC A2 não é termometria da câmara.
