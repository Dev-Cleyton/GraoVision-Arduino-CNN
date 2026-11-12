# Atualização experimental para o TCC — 10/11/2026

O mesmo termistor NTC de 10 kΩ, BETA nominal 3950 K, previamente localizado junto à região da célula de carga, foi **transferido para o interior da câmara de aquecimento**, mantendo a ligação à entrada analógica A2 do Arduino Uno. Por isso, as leituras emitidas pelo firmware a partir dessa alteração passam a representar a temperatura no **ponto de instalação dentro da câmara**. Elas não representam temperatura dos grãos nem homogeneidade de temperatura do volume da câmara, e ainda não foram calibradas contra instrumento independente.

A versão integrada atual é **V10.2.2**, que NÃO aciona o aquecedor. O sketch V10.2.3 incluído neste pacote foi criado exclusivamente para leitura passiva da temperatura e não participa da automação P1 → CNN → P2 → PF. O ensaio de secagem real e a determinação de umidade continuam pendentes de: proteção térmica independente testada, circuito de aquecimento dimensionado, calibração de temperatura, método de referência para perda de água e condições de segurança mecânica do elevador.

## Campos sugeridos para ficha

| Campo | Valor |
|---|---|
| Sensor | Termistor NTC, 10 kΩ @25 °C (a conferir no componente) |
| Constante BETA | 3950 K (presumida, a confirmar) |
| Pino Arduino | A2 |
| Resistência fixa do divisor | 10 kΩ (confirmar por medição) |
| Local de medição | Interior da câmara (posição exata a fotografar) |
| Aquecedor | Desligado/isolado; sem teste real de desidratação |
| Referência externa | A preencher após conferência com instrumento independente |
