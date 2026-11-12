# V10.2.2 — Gestão de erros + retorno supervisionado

**Comece por:** `LEIA_PRIMEIRO_V10_2_2.md`.

**Atenção ao último teste:** se o log mostrar `EEPROM:POS_COMANDADA:14000:ANGULO:180` e o equipamento estiver no alto, **não envie `CONFIRMO-X`**. Essa confirmação não movimenta a bandeja.

Entregáveis: firmware real e simulado em `arduino/`; Python completo em `analise_graos/`; matriz de tratamentos em `MATRIZ_TRATAMENTO_DE_ERROS_V10_2_2.md`.

Aquecimento permanece bloqueado; não foi conectado nem testado. Não há homing físico nem freio independente comprovado. Todo movimento de recuperação exige confirmação presencial e mantém o processo como erro no relatório.
