# GraoVision - V10.1C - pacote de pre-producao

## Proveniencia e escopo

Inventario gerado a partir de ZIPs locais; nenhuma execucao ou validacao de hardware foi feita.
Foram copiados somente arquivos-fonte e textos tecnicos selecionados, preservando seus bytes
no repositorio local. Os nomes e caminhos internos originais constam no manifesto CSV
junto com o destino exato no repositorio. Caminhos longos recebem nome curto
deterministico em `__caminhos_longos/` (sem alterar o arquivo original).

| ZIP original (relativo a pasta de arquivos) | Fontes importadas | Ignorados | `.env` ignorados | SHA-256 do ZIP |
|---|---:|---:|---:|---|
| `Dezimo Quarto/PACOTE_TESTE_TOTAL_PRE_PRODUCAO_V10_1C.zip` | 3 | 1 | 0 | `0e514cd435ddd7cfd07be2d3a188b19eb75584cd5c02855695a4c5805758a48a` |

## Notas de seguranca

- Arquivos `.env`, chaves privadas, tokens, bases, imagens, logs, relatorios binarios e ambientes virtuais nao sao importados.
- Pacotes com zero fontes selecionadas continuam relacionados acima e permanecem nos ZIPs originais.
- O verificador de credenciais e estatico e incompleto; a publicacao exige auditoria manual de todo o Git e dados pessoais.
- As fontes nao foram alteradas intencionalmente pelo importador; o Git pode normalizar finais de linha segundo a configuracao local.
- Os titulos dos pacotes indicam a finalidade declarada, nao comprovam resultados experimentais.
- Arquivos graficos, evidencias de ensaio e dados tabulares precisarao de triagem especifica antes de eventual publicacao.
