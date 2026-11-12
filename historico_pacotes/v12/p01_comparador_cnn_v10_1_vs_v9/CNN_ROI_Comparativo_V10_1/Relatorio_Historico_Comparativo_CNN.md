# Auditoria complementar — experimentos de ROI e contagem da CNN

**Projeto:** Arduino + Python + CNN, classificação de grãos

**Objetivo:** recuperação de evidências históricas existentes na V8, V9 e V10.1, sem novas chamadas à API Roboflow.

## 1. Fontes usadas

- `Todos.zip`, V8 e V9: códigos `validar_contagem_v8.py`, `testar_roi_v9.py`, imagem de referência `imagem_20_graos.jpeg`.
- `diagnostico_20_graos.zip`: `deteccoes_20_graos.csv` (inferência antiga da imagem completa).
- `diagnostico_v9.zip`: `roi_deteccoes.csv` (inferência antiga do recorte da imagem de referência).
- `diagnostico_iluminacao.zip` e `diagnostico_misto.zip`: CSVs históricos de inferência de outras imagens.
- Firmware e software V10.1 no pacote anteriormente enviado e logs dos dois testes reais de 7/11/2026.

A imagem histórica original usada pelos testes V8 e V9 foi conferida pelo conteúdo e pelas imagens anotadas, de mesmo tamanho 1152 × 2048 pixels. A ROI V9 corresponde a centro (560,980), raio 445 pixels.

## 2. Contagens de detecções recuperadas

| Ensaio | Brutas | Na ROI | good | bad | Referência manual | Erro numérico absoluto |
|---|---:|---:|---:|---:|---:|---:|
| V8 imagem completa + aplicação offline da ROI V9 | 32 | 11 | 11 | 0 | 20 | 45% |
| V9 recorte+máscara, mesma foto original | 5 | 5 | 5 | 0 | 20 | 75% |
| V9 iluminação modificada, outra foto | 20 | 20 | 20 | 0 | 20 | 0% |
| V9 misto, outra foto | 20 | 20 | 20 | 0 | 20 | 0% |

**Nota de metodologia:** o CSV original V8 não contém campo `dentro_roi`; neste relatório os 11 itens aceitos foram calculados **posteriormente** sobre as 32 previsões, com o critério de centro `(x−560)²+(y−980)² <= 445²`. Isso não é um novo teste da CNN. Todos os CSVs foram gerados historicamente e fornecidos pelo usuário.

**Limitações:** contagem numérica coincidente com 20 não comprova identificação um-para-um, ausência de duplicatas ou rótulos corretos. A imagem “misto” foi classificada com todas as 20 detecções como `good` nos dados encontrados; não se pode concluir que os 20 grãos fossem realmente bons sem referência manual das classes. A iluminação e distribuição dos grãos diferem entre os três arquivos de imagem, impedindo comparações causais diretas.

## 3. V10.1 recente, relatórios fornecidos pelo usuário

| Execução | Brutas | Na ROI | bad | Percentual de bad | Decisão do software |
|---|---:|---:|---:|---:|---|
| 20261107_184754_3c0389 | 116 | 82 | 2 | 2,44% | `CAM:SUCESSO`, sequência de pesagens concluída |
| 20261107_192121_598cf4 | 46 | 10 | 7 | 70,00% | `CAM:RUIM`, ciclo encerrado sem P2 |

O código de integração responde corretamente conforme o percentual *das detecções aceitas*. A validação por inspeção manual dos grãos ainda é necessária.

## 4. Conclusão e decisão recomendada

1. Não substituir a V10.1 pela V9 com base apenas nos CSVs existentes.
2. Comparar a **mesma fotografia original** com o mesmo modelo e configurações nas três condições: completa, recortada sem máscara e recortada com máscara.
3. Reaproveitar as ferramentas históricas e registrar a contagem manual, marcação individual e os erros de classe em um conjunto de referência.
4. Só então considerar alteração do mecanismo de ROI e realizar validação independente da CNN.
5. Não alterar o firmware Arduino, protocolo serial ou fluxos de aprovação já testados enquanto a comparação permanece offline.

## 5. Reprodutibilidade

Este pacote inclui um script comparador e testes offline que não dependem de chave Roboflow. A etapa de inferência das três condições deve ser executada no PC do usuário. Não foram inventadas métricas de acurácia, precisão, recall ou desempenho do modelo.
