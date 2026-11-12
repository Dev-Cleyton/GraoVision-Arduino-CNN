# GrãoVision — Cadastros e relatórios de amostras (TCC)

Esta revisão expande o **GrãoVision com três temas** e preserva o firmware, o protocolo Arduino/UNO, os movimentos, a CNN, a EEPROM e os limites térmicos. A GUI continua com aquecimento real desabilitado. Não é necessário reinstalar/gravar o Arduino para usar os cadastros.

## Instalar e iniciar no Windows 10

1. Extraia o ZIP em uma pasta **nova** para preservar sua versão anterior funcional.
2. Copie o arquivo `analise_graos\.env` da versão anterior para `analise_graos\.env` desta revisão (não compartilhe sua chave Roboflow).
3. Execute **`INSTALAR_DEPENDENCIAS.bat`**. Foi acrescentada a biblioteca `reportlab` para gerar PDFs.
4. Execute **`INICIAR_GRAOVISION.bat`**. `main.py` continua abrindo a interface gráfica; `main.py --terminal` executa o terminal legado.
5. A GUI continua oferecendo **Esmeralda**, **Fluent Azul** e **Operador**.

## Fluxo completo de cadastro

1. Clique **Fornecedores / Clientes**, aba **Fornecedores**. Cadastre a empresa/produtor que forneceu o grão e clique **SALVAR CADASTRO**.
2. Vá à aba **Clientes**. Cadastre o cliente/interessado no relatório e clique **SALVAR CADASTRO**. O fornecedor e o cliente podem representar a mesma pessoa, mas devem possuir cadastro de cada função.
3. Vá à aba **Amostras e lotes**. Preencha código, fornecedor, cliente, grão/produto, lote, chegada (data e hora), coleta (data e hora) e responsável pela coleta. Preencha os outros campos quando disponíveis.
4. Clique **SALVAR CADASTRO** e depois **USAR NA ANÁLISE**. Isso vincula a amostra ao próximo ciclo.
5. Volte à **Visão geral** e inicie um ciclo sem aquecimento, seguindo as permissões/confirmacões do Arduino já existentes.
6. Ao encerrar, a análise é armazenada no banco SQLite com **snapshot das partes e da amostra**, mesmo se houver falha no processo. A geração do PDF não envia comandos ao Arduino.
7. Abra **Relatórios / Imprimir**, selecione uma análise e clique **GERAR / ABRIR PDF**. Para escolher uma impressora, use Ctrl+P no visualizador. O botão **IMPRIMIR** tenta usar a impressora padrão do Windows e solicita confirmação.
8. Use **BACKUP CADASTROS** para salvar uma cópia do SQLite em outro local.

## Dados capturados

- **Fornecedor e cliente:** razão social/nome, nome fantasia, CPF/CNPJ declarado, inscrição estadual, telefone, e-mail, responsável, endereço, bairro, município, UF, CEP e observações.
- **Amostra/lote:** código, produto, variedade, safra, lote, propriedade/origem, município/UF, nota fiscal, placa/transporte, peso declarado do lote e amostra, chegada/recebimento (data e hora), coleta/amostragem (data e hora), coletor, local/método de coleta, lacre, embalagem, finalidade, norma de referência informada e observações.
- **Análise:** identificador único e horário da sessão, status final, resultado experimental da CNN, detecções válidas/ruins, P1, P2, PF, P3, P4, P5, temperatura do NTC, estado do comando do aquecedor, condição da umidade e erro informado, caso exista. Imagem anotada da CNN é incluída no PDF se o arquivo original estiver disponível.

## Onde fica armazenado

O cadastro e os dados pessoais ficam **somente no seu computador**, em:

`%USERPROFILE%\.graovision_tcc\rastreabilidade.db`

Os PDFs produzidos ficam em:

`%USERPROFILE%\.graovision_tcc\relatorios_pdf\`

O programa de cadastro não sincroniza esses dados com serviços externos. O banco SQLite não é criptografado; proteja o login, a pasta e os backups. Não salve o banco em pasta pública. Os resultados da CNN continuam usando o serviço Roboflow já existente (imagem da amostra), conforme as credenciais do seu projeto.

**Atenção:** cadastros de fornecedor, cliente e amostra devem ser feitos **antes de iniciar** a análise para o relatório sair preenchido. Ciclos anteriores a esta revisão não são importados automaticamente. Ciclos sem cadastro continuam funcionando, mas ficam identificados como **SEM CADASTRO** e seu PDF não conterá dados inventados de cliente ou fornecedor.

## Referências normativas e limites de validade

Este produto é um **protótipo de TCC**. O PDF é um **relatório técnico experimental de rastreabilidade**, não um Certificado de Classificação Vegetal oficial, documento fiscal, laudo de umidade validado ou documento emitido por classificador credenciado.

A documentação oficial deve ser verificada conforme o grão, tipo de estabelecimento, finalidade e regras vigentes. Consultar:

- Lei nº 9.972/2000 — classificação de produtos vegetais: https://www.planalto.gov.br/ccivil_03/leis/l9972.htm
- Decreto nº 12.709/2025 — regulamento mais recente (o Decreto nº 6.268/2007 foi revogado): https://www.planalto.gov.br/ccivil_03/_ato2023-2026/2025/decreto/D12709.htm
- Para **milho**, IN MAPA nº 60/2011, regulamento técnico específico; verificar atualizações: https://sistemasweb.agricultura.gov.br/sislegis/action/detalhaAto.do?chave=1739574738&method=visualizarAtoPortalMapa
- Credenciamento e habilitação de classificadores, orientações atuais: https://www.gov.br/agricultura/pt-br/assuntos/inspecao/produtos-vegetal/registro-importacao-e-exportacao-1/registro/registro-de-estabelecimentos

Os campos de identificação/rastreabilidade disponíveis não são garantia de preenchimento integral dos requisitos normativos. A própria norma do grão poderá exigir outras informações, amostragem representativa, parâmetros e documentos específicos.

## Testes feitos nesta revisão

- 84 testes automatizados Python aprovados (81 anteriores + 3 novos de rastreabilidade/relatórios).
- SQLite: criação, edição, validação de campos, vinculação cliente/fornecedor, snapshot histórico e geração de PDF.
- PDF de exemplo renderizado e conferido em 2 páginas.
- Interface Tk aberta com telas de cadastro/relatório, sem Arduino conectado.
- **Ainda pendente:** teste de impressão em impressora física no seu Windows e validação do ciclo integrado com Arduino nessa revisão.
