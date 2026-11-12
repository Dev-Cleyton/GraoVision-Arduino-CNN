# V02 - Segunda Versao do GraoVision

## Origem

Arquivos preservados da pasta historica Segunda Versao_2.

Fontes:
- TCC -HARDWRE -SOFTWARE.zip
- correcao1_serial_analise_graos.zip

## Comparacao com a V01

Foram comparados 19 arquivos do ZIP principal.

Resultado:
- 4 arquivos modificados;
- 15 arquivos inalterados;
- nenhum arquivo novo ou removido nessa comparacao.

Arquivos modificados:
- core/analysis_service.py
- core/sample_service.py
- hardware/arduino_handler.py
- main.py

Os dois firmwares Arduino permaneceram identicos
aos respectivos arquivos do ZIP principal da V01.

## Correcao serial separada

O ZIP correcao1_serial_analise_graos.zip contem
cinco arquivos Python.

Quatro arquivos sao identicos aos do ZIP principal
da V02.

A unica diferenca identificada esta no metodo
_enviar_mensagem, na linha 197 de arduino_handler.py.

Alteracao textual encontrada:
- original: seta ASCII ->
- revisao: seta Unicode U+2192

A alteracao pode causar problemas de exibicao em
terminais Windows configurados com cp1252.

Nao ha evidencia, nesta comparacao, de correcao
funcional do protocolo serial.

## Organizacao no repositorio

- firmware/historico/v02_segunda_versao/
- software/historico/v02_segunda_versao/analise_graos/
- software/historico/v02_segunda_versao/correcao1_serial/

## Seguranca

O arquivo .env nao foi importado.

Na copia de main.py, a impressao de uma variavel
de ambiente foi substituida por uma mensagem segura.

A revisao geral de credenciais permanece pendente.

## Evidencias e limitacoes

A comparacao entre arquivos foi realizada localmente.
O main.py passou na verificacao de sintaxe Python.

Nao foram realizados novos testes com o hardware
Arduino nesta etapa de organizacao do Git.

As datas internas dos arquivos ZIP nao devem ser
interpretadas automaticamente como datas dos ensaios.
