# V03 - Terceira Versao do GraoVision

## Origem

Arquivos provenientes da pasta Terceira Versao_3.

Pacotes historicos:
- TCC -HARDWRE -SOFTWARE.zip
- Fase_1_Tara_Arduino_Python.zip

## Inventario

Foram importados:
- 19 arquivos do ZIP principal;
- 3 arquivos do pacote especifico de tara.

Total: 22 arquivos.

Ambientes virtuais, arquivos .env e dependencias instaladas
nao foram incluidos na importacao.

## Evolucao em relacao a V02

A comparacao dos ZIPs principais identificou modificacoes em:
- core/analysis_service.py
- hardware/arduino_handler.py

O firmware codigo_automacaoV2.ino da V02 nao aparece
no ZIP principal da V03.

A V03 contem codigo_automacaoV3.ino.

Os demais 16 caminhos comparados permaneceram inalterados.

## Analise das funcoes Python

Em analysis_service.py:
- iniciar_sistema foi modificado.

Em arduino_handler.py:
- _enviar_mensagem foi modificado;
- _ler_mensagem foi modificado;
- realizar_tara foi adicionado.

Essas diferencas foram identificadas por comparacao
estatica do codigo Python.

## Pacote Fase 1 - Tara

O pacote especifico contem:
- hardware/arduino_handler.py
- core/analysis_service.py
- codigo_automacaoV2/codigo_automacaoV2.ino

Comparado com a V03 principal:
- analysis_service.py e identico;
- arduino_handler.py difere no metodo _ler_mensagem;
- o firmware da fase de tara possui o mesmo conteudo
  de codigo_automacaoV3.ino, embora tenha outro nome.

Os arquivos da fase de tara foram preservados
separadamente para manter a rastreabilidade historica.

## Organizacao

Firmware:
- firmware/historico/v03_terceira_versao/

Software:
- software/historico/v03_terceira_versao/

## Seguranca

O arquivo .env original nao foi importado.

A copia de main.py deve manter os valores de ambiente
ocultos nas mensagens de diagnostico.

A auditoria de credenciais e preliminar e nao substitui
a revisao de seguranca antes da publicacao.

## Evidencias e limitacoes

Foram realizadas comparacoes de arquivos e analises
estaticas do codigo Python.

A presenca do metodo realizar_tara nao comprova
a validacao fisica da tara no HX711.

Nao foram executados novos ensaios de hardware nesta
etapa de organizacao do repositorio.

Datas internas dos ZIPs nao devem ser utilizadas como
comprovacao automatica de datas de testes.
