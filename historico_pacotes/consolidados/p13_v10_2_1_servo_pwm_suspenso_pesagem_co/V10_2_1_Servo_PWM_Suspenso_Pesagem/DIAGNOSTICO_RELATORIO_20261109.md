# Diagnostico do relatorios.zip - ciclo 20261109_163309_58faee

- Resultado reportado: `ERRO_PROCESSO` / `ERRO:PESO_MONITOR_INSTAVEL`.
- CNN: 85 deteccoes aceitas na ROI, zero `bad`, dentro do limite visual (nao validado por contagem manual).
- Subida: sequencia de 0 a 14000 pulsos comandados presente no CSV. Nao ha descida registrada porque ocorreu a falha no monitoramento.
- P1: 16.6949 g. P2: 16.7721 g. Offset P2-P1: +0.0772 g.
- MEDICAO 0 s: 16.7721 g; 32 s: 16.6939 g; 60 s: 16.7436 g. NTC local: 25.07, 24.91, 25.82 C.
- Oscilacoes de amplitude reportadas no terminal: 0.2816, 0.2287, 0.2003, 0.3090 g.
- PF nao foi obtido; portanto nenhuma perda de massa/umidade pode ser calculada.
- A hipotese de interferencia do servo PWM e plausivel mas NAO confirmada por este unico ciclo, apesar de testes antigos do usuario com PWM suspenso.
- A suspensao de PWM nao desliga os 5V nem elimina possivel interferencia do driver, cabos, mecanica ou fonte compartilhada.
- Falha com elevador no alto precisa de protecao mecanica antiqueda; `STOP` do fluxo nao substitui parada de emergencia fisica.
