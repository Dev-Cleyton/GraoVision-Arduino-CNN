# GraoVision - V10.1 - integracao

## Proveniencia e escopo

Inventario gerado a partir de ZIPs locais; nenhuma execucao ou validacao de hardware foi feita.
Foram copiados somente arquivos-fonte e textos tecnicos selecionados, preservando seus bytes
no repositorio local. Os nomes e caminhos internos originais constam no manifesto CSV
junto com o destino exato no repositorio. Caminhos longos recebem nome curto
deterministico em `__caminhos_longos/` (sem alterar o arquivo original).

| ZIP original (relativo a pasta de arquivos) | Fontes importadas | Ignorados | `.env` ignorados | SHA-256 do ZIP |
|---|---:|---:|---:|---|
| `Dezimo Primeiro -v/V10_1_Integrada_Validacao.zip` | 20 | 1 | 1 | `1848655a8230db47a35f0aef1f451f91e683151a2ca44e7c335a1b9c47eb0f05` |

## Notas de seguranca

- Arquivos `.env`, chaves privadas, tokens, bases, imagens, logs, relatorios binarios e ambientes virtuais nao sao importados.
- Pacotes com zero fontes selecionadas continuam relacionados acima e permanecem nos ZIPs originais.
- O verificador de credenciais e estatico e incompleto; a publicacao exige auditoria manual de todo o Git e dados pessoais.
- As fontes nao foram alteradas intencionalmente pelo importador; o Git pode normalizar finais de linha segundo a configuracao local.
- Os titulos dos pacotes indicam a finalidade declarada, nao comprovam resultados experimentais.
- Arquivos graficos, evidencias de ensaio e dados tabulares precisarao de triagem especifica antes de eventual publicacao.
