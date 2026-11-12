# ROI AUTOMATICA V10.1A — CORRECAO V2

## Por que a primeira versao retornou `[REVISAR]`?

A V1 exigia pelo menos **95% do aro externo da bandeja** visivel. A foto do teste com bandeja baixa na imagem possui contorno exterior parcialmente cortado na borda inferior. O filtro impedia a identificacao, mesmo quando a **area interna de classificacao** permanecia inteiramente enquadrada.

Esta V2 permite cobertura exterior de **80%**, desde que a ROI interna seja **100% visivel** e que as bordas sejam confirmadas em pelo menos 2 setores angulares. Isso nao torna a identificacao infalivel: se o circulo nao for confiavel, retorna `REVISAR` e nao se deve classificar.

## Teste seguro — NO WINDOWS (sem Arduino nem Roboflow)

1. Extraia o ZIP dentro da pasta `V10_1_Integrada_Validacao`.
2. Abra o PowerShell nessa pasta e execute:

```powershell
& "C:/Program Files/Python313/python.exe" ".\ROI_Automatica_Experimental_V10_1A_V2\testar_roi_offline.py" --imagem ".\imagens\20261107_184754_3c0389\imagem_1.jpg" --saida ".\resultado_roi_automatica_v2"
```

3. Abra `resultado_roi_automatica_v2/01_imagem_1_ROI_AUTO.jpg` se o programa imprimir `[OK]`.
4. **Confirme visualmente** se a linha laranja acompanha a borda da bandeja e se todos os graos que precisam ser avaliados estao dentro do circulo ciano. Nao ha compensacao de bandeja inclinada ou oclusa.
5. Se imprimir `[REVISAR]`, envie a foto original `imagem_1.jpg` e o JSON da saida. A foto **sem anotacoes** e necessaria para diagnosticar o contorno.

## Integracao com V10.1 — SOMENTE DEPOIS DO TESTE OFFLINE

1. Guarde copia do projeto principal. Nao substitua a V10.1 original.
2. Na copia, copie `patch_para_analise_graos/core/roi_automatica.py` e `grain_classifier.py` para `analise_graos/core/`.
3. Acrescente `ROI_AUTOMATICA=1` ao `.env` local, sem compartilhar a chave Roboflow.
4. Rode com **movimentos e aquecedor desabilitados**. Se a deteccao da bandeja falhar, o processo deve ser interrompido como `ERRO_PROCESSO` — nunca aprovado ou reprovado pelos graos.

A V2 continua experimental: resultados preliminares de geometria offline, ainda sem medicao formal de acuracia. A V10.1, Arduino, pesos e CNN permanecem inalterados no teste offline. Fotos anotadas nesta distribuicao servem exclusivamente para diagnostico visual; nao sao a foto original do teste de 07/11.
