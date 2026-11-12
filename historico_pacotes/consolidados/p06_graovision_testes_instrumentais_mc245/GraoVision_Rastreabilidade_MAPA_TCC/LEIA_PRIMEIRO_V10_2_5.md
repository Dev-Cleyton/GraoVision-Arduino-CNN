# V10.2.5 — Controle térmico preparado + PF final medido

**ESTA VERSAO NAO ACIONA A LAMPADA.** O firmware fornecido tem
`AQUECEDOR_PERMITIDO_APOS_INSPECAO = false`, e o `.env.example` tem
`AQUECIMENTO_REAL=0`. O firmware e Python foram preparados para uma validação
futura após inspeção dos componentes e da instalação térmica independente.

## Arquivos

- `arduino/V10_2_5_Controle_Termico_Real/V10_2_5_Controle_Termico_Real.ino`:
  movimentos reais supervisionados; relé D7 continua bloqueado
- `arduino/V10_2_5_Controle_Termico_Simulacao/V10_2_5_Controle_Termico_Simulacao.ino`:
  movimentos simulados; relé D7 bloqueado
- `analise_graos/main.py`: executa Python e relatório completo
- `.env.example`: use sua chave própria Roboflow, sem compartilhá-la
- `VALIDACAO_OFFLINE_V10_2_5.txt`: testes já feitos, limites de verificação

## Correções em relação à V10.2.4

1. `P2` continua sendo adquirido ANTES do período de monitoramento.
2. Aos 120 s o firmware realiza uma **nova pesagem PF** e emite
   `MEDICAO:<tempo_REAL>:<PF>:<temperatura>` e `PF:<valor>`. Se a pesagem
   leva tempo adicional, `tempo_REAL` pode ser 121, 122 etc.; não é fabricado.
3. Prepara `AQUECEDOR:ON_COMANDADO:0`, `AQUECEDOR:OFF:<motivo>`,
   `AQUECEDOR:TEMPO_COMANDO_ON_MS:<ms>` e `RESFRIAMENTO:<s>:<C>`.
4. Prepara corte por limite termométrico de software, falha NTC, timeout
   e parada de operador. O programa Python envia heartbeat `PC:ALIVE`.
5. Depois do calor, servo/elevador só retomam depois do resfriamento no NTC
   **e confirmação presencial** de que todas as partes estão seguras.
6. Adiciona `_eventos_termicos.csv` e `_resfriamento.csv` aos relatórios.
7. Mantém ROI CNN, EEPROM journal, PWM suspenso e P1/P2/PF.

## Como testar SEM aquecimento

1. Desconecte fisicamente a lampada H4 da fonte; confirme eixo vertical apoiado.
2. Abra o sketch Real no Arduino IDE e grave na placa (nao usar o Simulacao
   com movimentos reais).
3. Copie o `.env` da versao anterior preservando sua propria chave e COM4,
   com `MOVIMENTOS_REAIS=1` conforme o ensaio mecanico supervisionado
   e `AQUECIMENTO_REAL=0`.
4. Execute `analise_graos/main.py` e siga conferencias de X / tara / ARMAR.
5. O log deve mostrar `AQUECEDOR:BLOQUEADO_V10_2_5` e o P2 na posição de
   secagem; após cerca de 120 s deve emitir uma NOVA leitura PF.
6. Confirme `_resultado_tcc.md`, `_relatorio.json`, `*_pesagens.csv`,
   `_eventos_termicos.csv` e `_resfriamento.csv`.

## O que falta antes de permitir o relé físico

- Foto/identificação do dispositivo térmico recém-instalado: tipo (termostato
  NF rearmável ou fusível térmico), temperatura de atuação, corrente/tensão DC,
  local de fixação, instalação **em série no positivo 12 V da lâmpada**;
  verificar desconexão quando acionado sem depender do Arduino.
- Etiqueta legível da fonte KAP-337 com corrente nominal em 12 V. H4 usa
  um filamento de 55/60 W (cerca de 4,6 a 5 A nominal): a alegação comercial
  de 120 W não garante entrega de 5 A em 12 V e há pico de partida.
- Fusível DC junto da fonte e contatos do relé dimensionados em DC para a
  carga incandescente, corrente de partida, conexões e seção dos fios.
- Materiais da câmara, isolação, distância da lâmpada à bandeja, ponto mais
  quente, medição independente da temperatura e retenção do eixo vertical.
- O limite de 50 ºC no código é APENAS ILUSTRATIVO, **NÃO É TEMPERATURA
  DE SEGURANÇA DEFINIDA**. O valor de 35 ºC também é apenas critério provisório
  de resfriamento do NTC, não valida que o bulbo e os contatos estejam frios.

Quando estes itens forem demonstrados, o controle deve passar por revisão da
fiação e das condições térmicas, testes de falha da proteção e compilação real
no Arduino Uno antes do primeiro ensaio com a lâmpada energizada.

## Interpretação metrológica

`offset=P2-P1`, `PF_corrigido=PF-offset`, `variacao=P2-PF`,
`variacao_%=(P2-PF)/P2 * 100`.
Isso não calcula automaticamente umidade dos grãos. Foram observadas deriva e
leitura crescente SEM calor; aquecimento pode aumentar deriva térmica.
As contagens CNN não foram comparadas a rótulos manuais de referência.
