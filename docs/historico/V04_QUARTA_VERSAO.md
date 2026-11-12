# V04 - Quarta Versao do GraoVision

## Fontes

- ZIP principal `TCC -HARDWRE -SOFTWARE.zip`: 19 arquivos de codigo.
- ZIP complementar `Correcao_Fase2_Captura_Contagem.zip`: 2 arquivos de codigo.

## Diferencas entre ZIPs principais

Comparacao binaria dos arquivos reconhecidos; nenhuma execucao de codigo.

- MODIFICADO: `analise_graos/core/sample_service.py`
- MODIFICADO: `analise_graos/hardware/camera_handler.py`
- AUSENTE: `codigo_automacao/codigo_automacaoV3/codigo_automacaoV3.ino`
- NOVO CAMINHO: `codigo_automacao/codigo_automacaoV4/codigo_automacaoV4.ino`

## Pacotes complementares


### Correcao_Fase2_Captura_Contagem.zip

- `analise_graos/core/sample_service.py`: IGUAL ao ZIP principal.
- `analise_graos/hardware/camera_handler.py`: IGUAL ao ZIP principal.

## Preparacao para versionamento

- Arquivos com formatacao normalizada: 6.
- Impressoes de getenv protegidas: 1.
- `.env`, ambientes virtuais, dependencias instaladas e binarios nao foram importados.
- ZIPs originais preservados.

## Limitacoes e seguranca

- Analise estatica; nao comprova funcionamento ou testes fisicos.
- A verificacao automatica de segredos e limitada. Auditar todos os commits antes do GitHub.
- Datas internas dos ZIPs nao comprovam datas dos ensaios.
