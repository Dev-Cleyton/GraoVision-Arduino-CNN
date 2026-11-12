# Evidencias experimentais ja observadas - antes de aquecimento real

## Ensaio R3 integrado, 09/11/2026 (log fornecido pelo operador)
- P1: 10,8414 g; P2: 10,8994 g; PF: 10,9531 g.
- Offset de posicao = P2 - P1 = +0,0580 g.
- PF corrigido = 10,8951 g.
- Variacao P2 - PF = -0,0537 g (aumento aparente de leitura em vez de perda).
- CNN: 57 deteccoes aceitas, 4 `bad` (7,02%); fluxo aprovado para monitoramento sem calor.
- Comandos de motor: 14.000 pulsos de subida e 14.000 de descida; `FIM:1` confirmado.
- A operacao suave e a melhora de estabilidade foram relatadas pelo operador; nao ha encoder ou afericao mecanica de deslocamento.

## Ensaio V10.2, 09/11/2026 (log fornecido pelo operador)
- P1: 16,6949 g; P2: 16,7721 g; offset inicial +0,0772 g.
- ROI: 85 deteccoes aceitas, 0 `bad`, decisao da CNN aprovada.
- Temperaturas registradas de 25,07 °C, 24,91 °C e 25,82 °C; nao foram comparadas a termometro padrao.
- Monitoramento apresentou `PESO_MONITOR_INSTAVEL` e nao gerou PF.
- Nenhum ensaio apresentado ate aqui com aquecedor energizado e corte termico independente validado.

**Nota de consistencia:** nos relatórios anteriores a mudanca fisica do sensor, os dados do NTC se referem a posicao antiga. A partir da realocacao informada em 10/11/2026, os dados registrados em A2 se referem ao ponto de instalacao na camara. Nao misturar as duas localizacoes em uma mesma serie historica.
