# Ajuste final V10.2.7 — pausa entre transicoes

- Substituida a passagem direta `trocarEstado(X)` por `trocarEstadoComPausa(X)` **apenas no loop normal** do Arduino.
- Espera de 2000 ms (2 segundos) antes das transicoes normais, usando `esperarComStop`, que verifica STOP.
- Transicoes de falha/emergencia e comandos seriais nao receberam atraso.
- Em eventual ciclo com aquecimento comandado, a pausa adicional e suprimida para nao atrasar controles termicos; nesta compilacao o aquecedor e bloqueado (`AQUECEDOR_PERMITIDO_APOS_INSPECAO=false`).
- Preservados os 3 segundos de acomodacao apos servo/motor, os 5 segundos iniciais, os 15 segundos de estabilizacao e os 120 segundos de monitoramento.
- Preservados firmware real, simulacao e software Python, sem alterar calculos, EEPROM, ROI, CNN ou sequencia P1, P2, PF, P3, P4, P5.
- `delay(2)` = 2 milissegundos. Aqui foi implementada a pausa desejada de dois segundos, sem bloqueio cego do STOP.
- A alteracao e experimental: requer compilacao do sketch no Arduino IDE e validacao de bancada, com aquecedor desconectado e movimento supervisionado.
