# GrãoVision — Três temas selecionáveis para a IHM

Esta revisão mantém o backend Arduino + Python da **GrãoVision V10.2.7 R1** e acrescenta **somente a interface de escolha de três aparências**.

## Como executar (Windows 10, Python 3.13)

1. Extraia o ZIP mantendo a estrutura das pastas.
2. Se necessário, copie seu arquivo `analise_graos\.env` anterior para a pasta `analise_graos` deste pacote (sua chave Roboflow não deve ser compartilhada).
3. Execute `INSTALAR_DEPENDENCIAS.bat` caso ainda não tenha as bibliotecas instaladas.
4. Execute **`INICIAR_GRAOVISION.bat`** ou, pelo PowerShell, na pasta `analise_graos`: `& "C:\Program Files\Python313\python.exe" iniciar_interface.py`.
5. Na barra lateral clique **Aparência • 3 estilos** (não exige PIN).
6. Clique na prévia ou **SELECIONAR** no tema e depois **APLICAR E SALVAR APARÊNCIA**.
7. Volte a **Visão geral**. O tema será lembrado no próximo uso.

## Três modos

- **Esmeralda:** estilo verde-petróleo/ciano; painéis de pesagem, curva, câmera e operação em duas colunas.
- **Fluent Azul:** azul/índigo inspirado no Windows Fluent; três colunas, câmera central, leitura instantânea e linha do tempo à direita.
- **Operador:** grafite/verde, balança em destaque, orientações ao operador e controles supervisionados em colunas próprias.

As **miniaturas são mockups conceituais**. Os layouts realmente executados são Tkinter, e podem diferir em detalhes. Veja as capturas reais em `PREVIAS_DA_INTERFACE/`. As capturas foram obtidas sem Arduino conectado, usando **leituras históricas demonstrativas**, não representam uma sessão ao vivo.

## Melhorias incluídas

- As mensagens `VETOR:<etapa>:<i>:<ms>:<passos>:<massa>` atualizam a seção **Balança em tempo real**. Durante o monitoramento, `MEDICAO` também atualiza a leitura.
- Depois do fim de um ciclo, a tela mostra a **última leitura recebida**, e não finge existir monitoramento contínuo quando não há dados seriais.
- **Passe o cursor** sobre os pontos da curva térmica para visualizar o tempo e a temperatura exatos.
- A paleta dos botões ficou mais discreta, e a disposição dos painéis muda conforme o tema selecionado.
- Configurações são armazenadas localmente em `%USERPROFILE%\.graovision_tcc\config.json`.
- Temas podem ser trocados **somente quando não há ciclo nem confirmação pendente**.
- A página Aparência pode ser acessada por operador sem PIN; Configurações de operação continuam exigindo autorização administrativa.

## Segurança e limitações

Não modificamos arquivos `.ino`, comandos de atuadores, EEPROM, Roboflow, protocolos seriais ou limites térmicos nesta revisão. A IHM mantém `AQUECIMENTO_REAL=0`. Mover a posição X ou solicitar retorno continua exigindo autorização do firmware e conferência física, jamais apenas o último número de passos da EEPROM. STOP por software não substitui E-stop físico. **Não altere o firmware para contornar proteções da máquina.**

Não foi introduzida uma curva ideal de secagem: uma referência térmica exigiria perfil de processo validado; o gráfico atual mostra **somente medidas reais** e tooltip.

## Testes

Na pasta `analise_graos` execute:

```powershell
& "C:\Program Files\Python313\python.exe" -m pytest -q
```

Foram aprovados **81 testes automatizados Python**, além de ensaio gráfico de troca dos três temas e atualização das leituras sem conexão ao hardware. Teste físico supervisionado ainda é necessário.
