# GrãoVision — pendências para divulgação pública

> **Estado:** documento de acompanhamento. Este checklist não constitui aprovação de segurança nem autorização de publicação.

## Verificações concluídas anteriormente

- [x] Histórico de 19 commits organizado retrospectivamente; e-mails de autoria e commit verificados como `noreply` na auditoria anterior.
- [x] 19 alertas de atribuições classificados: 10 leituras de ambiente, 5 parâmetros, 3 expressões não conclusivas para segredo fixo e 1 exemplo de documentação.
- [x] Exemplo de documentação `ROBOFLOW_API_KEY=SUA_CHAVE_REAL` identificado como marcador, não chave real.
- [x] Usuário confirmou que as fotografias utilizadas no treinamento são de sua autoria.

Esses itens são **resultados de uma análise limitada**, não garantem que o histórico esteja livre de segredos ou de conteúdos de terceiros.

## Pendências para tornar o repositório público

- [ ] Revisar direitos de distribuição de todo código e eventuais contribuições de terceiros, inclusive código copiado ou adaptado.
- [ ] Conferir termos aplicáveis ao modelo, pesos, exportações, imagens e dataset no Roboflow; fotografias próprias não garantem licença do serviço ou do modelo exportado.
- [ ] Revisar manualmente os arquivos CSV/JSON e capturas, sobretudo dados identificáveis, relatórios e metadados.
- [ ] Conferir origem, versão e licença das dependências: `numpy`, `opencv-python`, `pillow`, `pyserial`, `python-dotenv`, `reportlab`, `roboflow`.
- [ ] Revisar o histórico Git completo, inclusive variantes antigas, antes de alterar a visibilidade para `Public`.
- [ ] Decidir a licença **somente para o código que você tem direito de licenciar**. Não adicionar licença de terceiros sem verificar seus requisitos.
- [ ] Fazer backup **externo** dos ZIPs originais e dos trabalhos ainda não versionados antes de formatar o computador.
- [ ] Anexar documentação de ensaios e fotos da bancada apenas com autorização e evidências verificáveis.

## O que as imagens mostram

As imagens em `docs/imagens/` são capturas de interface fornecidas no contexto do projeto. Gráficos e indicadores mostrados nelas não são validação metrológica, prova de acurácia da CNN, nem substituem os logs brutos.

## Segurança operacional

Firmware histórico é código experimental. **Não acionar aquecimento, relés ou atuadores** com base somente na existência de um sketch versionado. Conferir pinagem, intertravamentos, sensores e proteção térmica em bancada supervisionada.
