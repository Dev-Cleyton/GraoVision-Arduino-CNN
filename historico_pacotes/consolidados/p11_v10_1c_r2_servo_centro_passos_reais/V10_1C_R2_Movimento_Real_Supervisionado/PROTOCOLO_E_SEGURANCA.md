# Protocolo serial V10.1C-R2 (9600 baud)

`READY:V10.1` mantém compatibilidade com os módulos Python da V10.1B/C.

## Inicialização

`V10.1C-R2:REAL_SUPERVISIONADO_SEM_AQUECIMENTO` ou `V10.1C-R2:SIMULADO_SEM_AQUECIMENTO`; `MOVIMENTOS:REAIS_EXIGEM_ARMAR` ou `MOVIMENTOS:SIMULADOS_POR_PADRAO`; `AQUECEDOR:BLOQUEADO_V10_1`.

Se a EEPROM estiver incompleta/suja: `RECUPERACAO:PENDENTE:REPOSICIONAR_X_E_CONFIRMAR`. Somente após o operador verificar fisicamente e o Python enviar `RECUPERAR:X` vem `RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL` e `READY:V10.1`.

## Antes da tara (NOVO)

Python envia `ARMAR:REAL` somente quando `.env MOVIMENTOS_REAIS=1`, recebe `ARMADO:REAL`. Depois envia `CENTRALIZAR`; Arduino centraliza servo em 90 graus, retorna `SERVO:CENTRO:OK`; só então aceita `TARA`/`TARA:OK`.

O sketch simulado também exige `CENTRALIZAR` antes da tara, mas imprime `MOVIMENTO:SIMULADO:SERVO:90` e não aciona o motor.

## Ciclo

`START` → `P1:<g>` → servo câmera 0° → `CMD:ACTIVATE_CAMERA` → `CAM:SUCESSO` ou `CAM:RUIM` → se aprovada, centro 90° → subir 14.000 passos → servo secagem 180° → `P2:<g>` → `MEDICAO:<seg>:<peso>:<temperatura_local>` → `PF:<g>` → centro 90° → descer 14.000 passos → `FIM:1`.

## Passos (NOVO)

`PASSOS:<SUBIDA|DESCIDA>:<pulsos_enviados_neste_movimento>:<total>:<posicao_logica>`

Exemplos: `PASSOS:SUBIDA:500:14000:500` e `PASSOS:DESCIDA:500:14000:13500`.

As mensagens saem na inicialização de cada movimento, a cada 500 pulsos e no final. Correspondem a PULSOS COMANDADOS, não a realimentação de encoder.

`ESTADO?` → `STATUS:ETAPA:<n>:PASSOS:<p>:ANG:<graus>:ARMADO:<0|1>`.

## Limitações

`STOP` fecha o processo e desativa relés; quando o suporte vertical depende da energia do driver para se sustentar, isso pode deixar a carga cair. Não substitui botão/circuito de parada de emergência e retenção independente. `AQUECEDOR:BLOQUEADO_V10_1` permanece obrigatório. Não executar automaticamente depois de erro de EEPROM, reset ou falta de energia sem reconfirmar a posição física.
