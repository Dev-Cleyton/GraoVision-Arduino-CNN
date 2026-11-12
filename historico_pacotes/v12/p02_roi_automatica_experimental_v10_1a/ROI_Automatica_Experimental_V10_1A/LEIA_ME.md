# ROI automática experimental V10.1A — TCC de classificação de grãos

## Objetivo

Manter webcam e posição do equipamento como estão. Em cada captura, o Python deve localizar **a borda circular da bandeja** e calcular um círculo interno de análise. A CNN continua `kernel-corn`, versão 54, confiança 28, overlap 24. **Não altera Arduino nem atuadores.**

## Funcionamento

- OpenCV reduz temporariamente a foto para detectar o círculo exterior da bandeja (HoughCircles).
- Verifica se a borda está suficientemente nítida e se quase todo o aro aparece na imagem.
- Usa 81% do raio exterior como ROI interna. Esta é uma proporção experimental, NÃO um valor validado para todas as bandejas.
- O Roboflow recebe a fotografia completa (igual à V10.1 original). Apenas a posição da ROI usada para filtrar detecções é atualizada.
- Salva um JSON com as coordenadas detectadas junto da fotografia.
- Se não localizar a bandeja de forma confiável, **não usa a ROI fixa como substituição automática**. No programa integrado, a falha é registrada como `ERRO_PROCESSO` e o Python solicita parada do Arduino. Não é reprovação dos grãos.

## Etapa A — testar offline, sem Arduino e sem Roboflow

Coloque os arquivos `roi_automatica.py` e `testar_roi_offline.py` na mesma pasta; o pacote já tem esses arquivos na raiz. No PowerShell, dentro dela:

```powershell
& "C:/Program Files/Python313/python.exe" ".\testar_roi_offline.py" --imagem "C:\CAMINHO\PARA\imagem_1.jpg" --saida ".\resultado_roi"
```

Abra a imagem gerada `*_ROI_AUTO.jpg`: linha **laranja** = aro exterior detectado; **ciano** = ROI interna que será aplicada. Examine se **todos os grãos da bandeja** ficaram dentro da linha ciano, mesmo com mudanças de posição. Se houver grãos perto da borda e fora da ROI, a proporção 0.81 deve ser revista a partir das imagens, nunca aprovada automaticamente.

O script retorna código 2 quando a geometria não é segura. Isso é esperado quando a bandeja está cortada ou mal visível.

## Etapa B — ativar experimentalmente na V10.1 existente

**Faça uma cópia da pasta V10.1 que já funciona antes de qualquer mudança.**

1. Copie `patch_para_analise_graos/core/roi_automatica.py` para `V10_1_Integrada_Validacao/analise_graos/core/`.
2. Copie `patch_para_analise_graos/core/grain_classifier.py` para o mesmo lugar, substituindo SOMENTE na cópia de testes.
3. No arquivo `.env` local da cópia, ACRESCENTE a linha `ROI_AUTOMATICA=1` (mantenha a sua chave Roboflow em sigilo).
4. Execute o `main.py` normal. A câmera deve produzir `[ROI_AUTO] Bandeja: centro=(...); raio interno=...` antes da inferência.
5. Se a bandeja não for detectada, o programa gera `ERRO_PROCESSO` com o motivo, não envia aprovação e não inicia secagem.
6. Para restaurar a ROI fixa, altere `ROI_AUTOMATICA=0` (ou remova a variável) na cópia e reinicie o Python.

**Atenção:** o `ROI_AUTOMATICA` é lido do processo do Python, que na V10.1 carrega `.env` por `python-dotenv`. Não envie seu `.env` a terceiros.

## Limitações / critérios de aceitação

- O protótipo detecta **círculos**, não elipses muito inclinadas nem bandejas retangulares.
- A webcam precisa enxergar a bandeja inteira e boa parte do aro. Objetos/estruturas circulares semelhantes podem causar falsos positivos; é necessário validar com imagens do seu hardware.
- A ROI automática não corrige sombras, reflexos, falsas detecções, classes erradas ou grãos sobrepostos.
- A fotografia anotada (`*_detectada.jpg`) serve para conferência visual; no teste offline use sempre a fotografia **original**, sem caixas nem círculos desenhados.
- A decisão `bad > 25%` continua baseada nas *detecções* da CNN, não em contagem manual certificada.
- Proibido habilitar aquecedor durante estes testes. Não alterar pinagem do Arduino, movimentos ou segurança térmica.

## Evidências offline obtidas nesta preparação

| Fotografia histórica | Bandeja detectada | Centro estimado | Raio interno (px) |
|---|---|---|---|
| `imagem_1(6).jpg` | SIM | (1001, 620) | 400 |
| `imagem_1(7).jpg` | SIM | (1004, 609) | 400 |
| `imagem_1(8).jpg` | SIM | (1006, 612) | 399 |
| `imagem_1(4).jpg` | NAO: requer revisão | — | — |

Estes são resultados geométricos offline sobre fotos históricas de 2048×1152 px. Não há novas inferências Roboflow nem métrica de acurácia do detector neste pacote. O exemplo não comprova funcionamento em todas as posições do sistema.
