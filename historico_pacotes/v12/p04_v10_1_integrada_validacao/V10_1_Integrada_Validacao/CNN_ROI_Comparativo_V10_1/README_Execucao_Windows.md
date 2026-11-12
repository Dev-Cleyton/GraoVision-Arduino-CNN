# Comparação experimental CNN V10.1 × V9 — Sem alterar a máquina

**Objetivo:** comparar três condições de entrada do mesmo arquivo de imagem no mesmo modelo Roboflow `kernel-corn`, versão `54`, confiança `28`, sobreposição `24`, com a **mesma ROI geométrica em coordenadas da imagem original**.

**Não altera:** Arduino, sketch V10.1, programa Python principal, classificação automática em produção, servo, driver, relés ou aquecedor.

## Como executar no Windows / PowerShell

1. Extraia a pasta `CNN_ROI_Comparativo_V10_1` **dentro** da pasta `V10_1_Integrada_Validacao`.
2. Abra o PowerShell na pasta `V10_1_Integrada_Validacao` (a mesma em que executou `analise_graos/main.py`).
3. Utilize o Python 3.13 já instalado, com bibliotecas `opencv-python`, `numpy`, `python-dotenv` e `roboflow`. Se necessário, instale no **mesmo** interpretador:

```powershell
& "C:/Program Files/Python313/python.exe" -m pip install opencv-python numpy python-dotenv roboflow
```

4. Primeiro gere imagens de conferência **sem chamar o Roboflow**:

```powershell
& "C:/Program Files/Python313/python.exe" ".\CNN_ROI_Comparativo_V10_1\comparar_cnn_roi_v101_v9.py" --imagem ".\imagens\20261107_184754_3c0389\imagem_1.jpg" --sem-inferencia --saida ".\teste_roi_visual"
```

5. Confira se o círculo está sobre a bandeja e inclui os grãos relevantes. Em caso positivo, execute a comparação **com inferência**. Sua chave deve existir no arquivo `.\analise_graos\.env` (NÃO inclua a chave no comando):

**Primeiro teste V10.1 — amostra visualmente aprovada:**

```powershell
& "C:/Program Files/Python313/python.exe" ".\CNN_ROI_Comparativo_V10_1\comparar_cnn_roi_v101_v9.py" --imagem ".\imagens\20261107_184754_3c0389\imagem_1.jpg" --env ".\analise_graos\.env" --saida ".\comparacao_cnn_aprovada"
```

**Segundo teste V10.1 — amostra visualmente reprovada:**

```powershell
& "C:/Program Files/Python313/python.exe" ".\CNN_ROI_Comparativo_V10_1\comparar_cnn_roi_v101_v9.py" --imagem ".\imagens\20261107_192121_598cf4\imagem_1.jpg" --env ".\analise_graos\.env" --saida ".\comparacao_cnn_reprovada"
```

**Ensaio histórico de 20 grãos — referência V9 com ROI específica:**

```powershell
& "C:/Program Files/Python313/python.exe" ".\CNN_ROI_Comparativo_V10_1\comparar_cnn_roi_v101_v9.py" --imagem ".\CNN_ROI_Comparativo_V10_1\exemplo_20_graos.jpeg" --manual-total 20 --roi-cx 560 --roi-cy 980 --roi-raio 445 --env ".\analise_graos\.env" --saida ".\comparacao_cnn_20_graos"
```

> **IMPORTANTE:** use os arquivos **originais** `imagem_1.jpg`, não os `imagem_1_detectada.jpg` (que já têm retângulos e texto). Os arquivos originais recentes estão no seu computador, não foram incluídos neste pacote. Caso os caminhos sejam diferentes, localize-os no Explorador do Windows.

## O que o script faz

Para cada fotografia original, executa três inferências independentes:

| Estratégia | Imagem enviada ao Roboflow | Origem |
|---|---|---|
| `V10_1_COMPLETA` | Imagem original, sem marcações | V10.1 atual |
| `RECORTE_SEM_MASCARA` | Recorte quadrado contendo a mesma ROI | Controle experimental adicional |
| `V9_RECORTE_MASCARA` | Mesmo recorte, com exterior da circunferência pintado de cinza 125 | Lógica da V9 |

As previsões das imagens recortadas são reconvertidas para o sistema de coordenadas da foto original, antes de aplicar a **mesma regra de filtro circular**.

Modelo e limiares são iguais nas três abordagens. Contudo, a escala da imagem de entrada muda entre foto inteira e recortes; por isso, a comparação avalia o efeito conjunto de recorte e entrada, não prova isoladamente uma causa.

**Saídas:** `COMPARACAO_FINAL.csv`, `COMPARACAO_FINAL.json`, um CSV de detecções por estratégia, uma imagem numerada por estratégia, os JSONs brutos das detecções e imagens de entrada da ROI.

Cada execução com inferência realiza **3 chamadas** à API Roboflow e pode consumir sua franquia. A opção `--sem-inferencia` não realiza chamadas.

## Resultados a interpretar

- `dentro_roi`: número de detecções aceitas, **não necessariamente número real de grãos**.
- `good`, `bad`: número de detecções classificadas por classe.
- `percentual_bad`: `bad / dentro_roi × 100`.
- `decisao`: decisão exploratória por percentual de detecções (limite provisório 25%).
- `erro_numerico_percentual`: diferença de **quantidade**, comparada a uma contagem manual fornecida com `--manual-total`. Não é acurácia de classificação.
- Sem confirmação visual das caixas e rótulos de cada grão, não calcular precisão/recall e não afirmar que 20/20 implica classificação correta.

O código de produção V10.1 continua inalterado. Não habilitar movimentos/aquecimento para este teste de software.

## Teste automático do script

```powershell
& "C:/Program Files/Python313/python.exe" ".\CNN_ROI_Comparativo_V10_1\testar_comparador_offline.py"
```

Esse teste exercita geometria, mascaramento, reconversão de coordenadas e modo offline; **não testa inferência Roboflow**.

## Segurança de credenciais

O arquivo `.env` fica somente na sua máquina. **Não compartilhe o `.env`, sua chave de API ou mensagens que a exponham.** O pacote não contém nenhuma credencial.
