# V10.2.3 — Diagnóstico NTC na câmara, SEM AQUECIMENTO

**Versão atual da automação integrada:** V10.2.2 — `V10_2_2_Retorno_Supervisionado_Real.ino`. Aquecedor permanentemente bloqueado nessa versão. Ao mover o NTC para a câmara, o valor enviado nos campos `MEDICAO:tempo:peso:temperatura` agora corresponde ao ambiente térmico no ponto do NTC — ainda sem calibração independente. NÃO representa mais a temperatura próxima à célula de carga.

**Este diagnóstico não é a versão de automação integrada.** É um sketch separado para caracterizar a leitura do NTC na câmara, sem alimentar o aquecedor.

## Antes de carregar o sketch diagnóstico

1. **Desconecte fisicamente a alimentação do aquecedor.** O módulo de relé ativo LOW não é garantia de segurança durante reset/boot.
2. **Posicione e trave mecanicamente o elevador em X inferior.** Este diagnóstico põe D6 em HIGH (OFF); se o motor sustenta carga vertical, desligá-lo com a máquina elevada pode provocar queda.
3. Desabilite alimentação de outros atuadores e confirme que ninguém pode ser atingido. Não há servo/STEP/EEPROM neste diagnóstico.
4. Confirme o divisor utilizado no firmware original: 5 V — resistor fixo 10k — A2 — NTC 10k (Beta3950) — GND. Se o divisor for invertido, a fórmula da temperatura estará incorreta.

## Teste de temperatura sem calor

- Abra `V10_2_3_Diagnostico_NTC_Camara_Sem_Aquecimento.ino` no Arduino IDE, placa Arduino Uno, porta correta. Verifique e carregue.
- Monitor Serial em 9600 baud. Exemplo: `NTC_CAMARA:2021:510.2:9949.3:25.11:LEITURA_NAO_CALIBRADA` (apenas ilustração).
- Registre pelo menos 5 minutos de temperatura ambiente com um termômetro de referência posicionado junto ao NTC e anote horário, valor NTC e valor referência.
- Se mostrar `ERRO:ADC_LIMITE...`, investigue conexões e divisor COM A ENERGIA DESLIGADA. Não energize o aquecedor.
- A faixa do NTC e seu encapsulamento, bem como cabos e isolamento, precisam ser compatíveis com a temperatura pretendida. O NTC e o Arduino não substituem a proteção independente de sobretemperatura.

## Para voltar ao software completo

- Restaure/carregue `V10_2_2_Retorno_Supervisionado_Real.ino` (na pasta original V10.2.2) e use o Python da mesma versão. **Não misture** o sketch diagnóstico com o `main.py` da automação.
- Após carregar o sketch integrado, respeite qualquer `RECUPERACAO:PENDENTE` da EEPROM; nunca confirme X com o elevador elevado.
- Não habilite D7 até que exista corte térmico independente, circuito de potência dimensionado/protegido, sensor de câmara validado, máquina fisicamente segura e liberação do responsável pelo equipamento.

## Para o relatório do TCC

Anote: modelo do NTC, Beta e R25 presumidos, resistor do divisor, posição dentro da câmara, cinco pares de leituras NTC/termômetro de referência, erro médio e foto do posicionamento. Declare a medição como preliminar até comparar com referência e condições reais de uso.
