# GrãoVision — Sistema experimental de análise de grãos

Projeto acadêmico de Engenharia de Computação: automação com Arduino Uno, aquisição de massa via HX711, monitoramento térmico por NTC, movimentação por servo e motor de passo, captura de imagens e detecção de grãos por YOLOv8n hospedado no Roboflow, integrado ao aplicativo Python GrãoVision.

> **Estado:** protótipo experimental. A execução de ciclos não equivale à validação metrológica ou à certificação de classificação oficial de grãos. A determinação quantitativa de umidade não foi validada.

## Navegação

| Área | Conteúdo |
|---|---|
| `firmware/` | Versões históricas do Arduino, seus códigos-fonte e notas de alteração |
| `software/` | Python, comunicação serial, câmera, IA e interface GrãoVision |
| `hardware/` | Arquitetura, pinagem, fotos autorizadas e diagramas |
| `ensaios/` | Protocolos, dados públicos anonimizados e análises reproduzíveis |
| `docs/HISTORICO.md` | Linha do tempo com datas comprovadas por fontes locais |
| `docs/MATRIZ_DE_VERSOES.csv` | Inventário de funcionalidades, evidências e resultados por versão |
| `docs/SEGURANCA_E_LIMITACOES.md` | Limites de segurança, dados pessoais e uso experimental |

## Arquitetura resumida

`Arduino Uno ↔ serial ↔ Python/GrãoVision ↔ webcam/Roboflow (YOLOv8n)`

- Arduino: HX711, NTC, servo, atuador e intertravamentos.
- Python/GrãoVision: aquisição, detecção, telemetria, registros e relatórios.
- Modelo hospedado: `kernel-corn`, versão `54`; classes `good` e `bad`.

## Rastreabilidade e cronologia

Este repositório poderá ser criado por **importação retrospectiva de arquivos históricos**. As datas dos commits de importação serão as datas verdadeiras em que os arquivos forem registrados no Git. Datas de desenvolvimento anteriores serão documentadas em `docs/HISTORICO.md`, sempre ligadas a evidências (logs, documentos, versões de arquivos ou commits originais recuperados). Arquivos sem data verificável serão identificados como `data não comprovada`.

## Como reproduzir

Verificar o `README.md` da versão específica em `firmware/` ou `software/` para dependências, ligação física, parâmetros e instruções de execução. Não energizar a H4 nem executar movimentos reais sem avaliação prévia da segurança mecânica e térmica.

## Licenças e dados

O código-fonte de terceiros, o detector hospedado, os dados de treinamento e fotografias externas podem possuir direitos e licenças diferentes. Nenhum direito de redistribuição é presumido. Credenciais, `.env`, dados pessoais e pesos privados não devem ser publicados.

## Referência acadêmica

Relatório de TCC II relacionado ao desenvolvimento experimental. Esta publicação do código não é uma certificação de qualidade, umidade ou conformidade ao MAPA.
