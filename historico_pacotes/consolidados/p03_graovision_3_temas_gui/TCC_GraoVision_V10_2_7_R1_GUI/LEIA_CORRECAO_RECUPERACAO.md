# R1 FIX — Recuperacao EEPROM em loop

**Problema:** quando MOVIMENTOS_REAIS=0 ou .env ausente, `AnalysisService.executar()` chamava `aguardar_pronto(recuperar_interativamente=False)` mesmo ao executar `main.py` com operador presente. A mensagem `RECUPERACAO:PENDENTE` gerava imediatamente `EEPROM requer verificacao fisica em X; inicio bloqueado`. Responder S apenas repetia a tentativa.

**Correcao:** na chamada de `aguardar_pronto`, usar `recuperar_interativamente=interativo`, em vez de `movimentos_reais`. A confirmacao manual continua obrigatoria. O Python so envia `RECUPERAR:X` depois de `CONFIRMO-X`. O comando apenas redefine a posicao LOGICA EEPROM, nao move o motor.

**Antes de usar:** manter aquecedor fisicamente desenergizado; conferir que a balanca esta no X inferior, servo fisicamente no centro 90°, prato vazio, mecanismo parado, sem obstaculos e sem risco termico. Nao confirmar se ha qualquer duvida.

**Como usar:** abrir `analise_graos/main.py`; ao detectar pendencia, o programa deve pedir `CONFIRMO-X`. Digitar somente depois da verificacao fisica. Aguarde `RECUPERACAO:LOGICA_X_GRAVADA:VERIFICACAO_MANUAL` e `READY:V10.1`.

**Nao alterado:** sketch Arduino, protecoes do aquecedor, limites de temperatura, movimentos, intervalos, CNN, logs e formula de pesagem. **Importante:** `MOVIMENTOS_REAIS=1` continua obrigatorio para habilitar movimentos reais no .env em ciclos posteriores, com ARMAR presencial.
