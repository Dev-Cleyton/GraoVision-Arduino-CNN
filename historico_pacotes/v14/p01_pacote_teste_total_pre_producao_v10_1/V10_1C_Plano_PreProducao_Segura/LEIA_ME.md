# Kit de preparação de teste total — V10.1C

Este kit NÃO altera firmware e NÃO habilita movimento físico ou aquecimento. Inclui um protocolo de aceitação e um analisador de relatórios V10.1B V2.

1. Leia `PLANO_ENSAIO.md` para entender as três portas: fluxo a frio (já validado); mecânica real (pendente de proteções); secagem/produção (pendente de segurança térmica e método de umidade).
2. Copie `resumir_ensaios.py` para dentro da pasta `analise_graos` da V10.1B V2.
3. Abra PowerShell nessa pasta, sem executar a máquina, e rode:

```powershell
& "C:/Program Files/Python313/python.exe" ".\resumir_ensaios.py" --pasta ".\relatorios" --saida ".\resumo_ensaios.csv"
```

4. Abra `resumo_ensaios.csv` para comparar os ciclos registrados, as duas ROIs, P1/P2/PF e resultados.

**Importante:** caso tenha desconectado intencionalmente a webcam, essa falha deve ser anotada como **falha provocada para validar segurança**, e não como problema recorrente do sistema.
