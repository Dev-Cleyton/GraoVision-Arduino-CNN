# Contrato serial Arduino ↔ Python V10.1 (9600, UTF-8/ASCII, LF)

## Sequencia sem falhas — CNN visualmente aprovada

```text
UNO: READY:V10.1
PC : TARA
UNO: TARA:OK
PC : START
UNO: P1:35.9000
UNO: MOVIMENTO:SIMULADO:SERVO:0
UNO: CMD:ACTIVATE_CAMERA
PC : CAM:SUCESSO
UNO: MOVIMENTO:SIMULADO:SERVO:90
UNO: MOVIMENTO:SIMULADO:SUBIDA
UNO: MOVIMENTO:SIMULADO:SERVO:180
UNO: P2:35.8900
UNO: AQUECEDOR:BLOQUEADO_V10_1
UNO: MEDICAO:0:35.8900:27.30
UNO: MEDICAO:30:35.8800:27.31
UNO: MEDICAO:60:35.8700:27.32
UNO: MEDICAO:90:35.8700:27.33
UNO: MEDICAO:120:35.8600:27.34
UNO: PF:35.8600
UNO: MOVIMENTO:SIMULADO:SERVO:90
UNO: MOVIMENTO:SIMULADO:DESCIDA
UNO: FIM:1
UNO: READY:V10.1
```

**Todos os numeros acima são exemplos ficticios de protocolo**, nao leituras do seu hardware.

## Sequencia CNN reprovada

```text
UNO: P1:35.9000
UNO: CMD:ACTIVATE_CAMERA
PC : CAM:RUIM
UNO: MOVIMENTO:SIMULADO:SERVO:90
UNO: FIM:REPROVADA_CNN
UNO: READY:V10.1
```

A causa (por exemplo `32/100 bad = 32%, limite 25%`) e escrita pelo **Python** com os dados reais do modelo Roboflow no JSON. O Arduino recebe so a decisao (evita sobrecarregar UART/flash).

## CNN inconclusiva ou erro

- Zero detecções válidas na ROI -> PC envia `CAM:INCONCLUSIVA` -> UNO envia `FIM:INCONCLUSIVA_CNN`. Nao relatar 'graos ruins' sem deteccoes.
- Falha de API/camera -> PC envia `STOP` -> UNO desliga relés e para sem tratar como reprovação visual.
- Qualquer comando `STOP` durante movimento cancela mais pulsos, relés desligam; **nao há retorno automatico a origem nem homing**. Necessario desligar/rever posição física e reiniciar sob supervisao.
- `ERRO:<motivo>` = terminal para ciclo; pode indicar falha de peso/camera/timeout.

## Semântica dos campos

| Campo | Unidade / regra |
|---|---|
| P1 | gramas, peso com suporte central antes de camera |
| P2 | gramas, referencia na posicao de secagem **se os movimentos forem reais** |
| MEDICAO:tempo:peso:temp | segundos; gramas; °C **NTC perto da celula** |
| PF | gramas, ultima pesagem da serie, mesma posicao de P2 |
| P2-P1 | diferenca de posicionamento; nao contabilizar como perda de agua |
| P2-PF | variacao de massa durante monitoramento; nao e teor de umidade |
| CAM:RUIM | CNN detectou `bad` acima de 25% das deteccoes validas na ROI |
| CAM:INCONCLUSIVA | zero deteccoes validas, ou seja, sem classificacao possivel |
| AQUECEDOR:BLOQUEADO_V10_1 | marcador obrigatorio: nao houve aquecimento |
| READY:V10.1 | pronto para tara de novo ciclo apos encerramento normal |

Sem controle automatico por `UMIDADE` no firmware, porque NTC não a mede. Nenhum comando habilita aquecedor nesta versão.
