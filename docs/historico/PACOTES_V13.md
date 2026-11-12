# GraoVision - V10.1B - ROI e camera multiciclos

## Proveniencia e escopo

Inventario gerado a partir de ZIPs locais; nenhuma execucao ou validacao de hardware foi feita.
Foram copiados somente arquivos-fonte e textos tecnicos selecionados, preservando seus bytes
no repositorio local. Os nomes e caminhos internos originais constam no manifesto CSV
junto com o destino exato no repositorio. Caminhos longos recebem nome curto
deterministico em `__caminhos_longos/` (sem alterar o arquivo original).

| ZIP original (relativo a pasta de arquivos) | Fontes importadas | Ignorados | `.env` ignorados | SHA-256 do ZIP |
|---|---:|---:|---:|---|
| `Dezimo Terceiro -V03/V10_1B_ROI_Automatica_Integrada.zip` | 25 | 1 | 1 | `7c85c24ace8939fdc64b90eff9f49602123989b802696b0145d3109c9ca69332` |
| `Dezimo Terceiro -V03/V10_1B_V2_Correcao_Camera_Multiciclos.zip` | 27 | 1 | 1 | `43d17d0550327f3c4b2aef4951467da36ac7b9a5b7d272b41db7b3492e352d8b` |

## Notas de seguranca

- Arquivos `.env`, chaves privadas, tokens, bases, imagens, logs, relatorios binarios e ambientes virtuais nao sao importados.
- Pacotes com zero fontes selecionadas continuam relacionados acima e permanecem nos ZIPs originais.
- O verificador de credenciais e estatico e incompleto; a publicacao exige auditoria manual de todo o Git e dados pessoais.
- As fontes nao foram alteradas intencionalmente pelo importador; o Git pode normalizar finais de linha segundo a configuracao local.
- Os titulos dos pacotes indicam a finalidade declarada, nao comprovam resultados experimentais.
- Arquivos graficos, evidencias de ensaio e dados tabulares precisarao de triagem especifica antes de eventual publicacao.
