# V10.2.7 — Ajuste de acomodação do servo e do motor

Base: V10.2.7 testada no log enviado. Sem alterar o protocolo P1/P2/PF/P3/P4/P5, as 12 leituras por vetor, o intervalo de 30 s ou o bloqueio D7.

## Mudanças no sketch

- START: 5 segundos antes de iniciar P1, para acomodar os grãos (estado ESPERA_P1, sem nova tara).
- Final de cada movimento REAL de servo: preserva o assentamento original (1200 ms; 2500 ms na centralização pré-tara) e adiciona 3000 ms de pausa antes da próxima transição.
- Final de cada subida/descida REAL do motor: adiciona 3000 ms de pausa, sem desenergizar o driver no topo.
- As pausas de 15000 ms antes de P2, P3, P4 e P5 permanecem inalteradas.
- Nenhuma gravação EEPROM é adicionada no percurso STEP. A função esperarComStop() continua consultando comandos serial STOP durante pausas, em incrementos de 5 ms.
- Mensagens adicionais: ACOMODACAO:SERVO:INICIO/FIM e ACOMODACAO:MOTOR:INICIO/FIM. Essas mensagens não substituem os eventos existentes.

ATENÇÃO: são pausas de tempo, **não detecção física de servo imóvel**. Se o servo continuar oscilando durante a pesagem, o movimento físico, alimentação, fixação e cabos precisam de inspeção. Não desprenda PWM no centro sem verificar sustentação mecânica.

## Testes seguros
1. Faça backup do firmware anterior e confirme que aquecedor está fisicamente desconectado.
2. Compile o sketch da pasta `arduino/V10_2_5_Controle_Termico_Real` na Arduino IDE para Uno e verifique bibliotecas Servo, EEPROM e HX711.
3. Teste primeiro o caminho de simulação e STOP. Faça ensaio físico apenas sob supervisão, com percurso e emergência conferidos.
4. Observe os eventos ACOMODACAO antes de P2/P3/P4/P5 e compare amplitude/desvio-padrão com a V10.2.7 anterior usando a mesma massa.
5. Não considere a medida validada só porque a pausa transcorreu; ruído e erro de posição ainda podem persistir.

Observação: o sketch de simulação mantém movimentação simulada, portanto os 3 s adicionais após movimentos se aplicam ao sketch real, não ao simulado; o estado ESPERA_P1 vale nos dois.
