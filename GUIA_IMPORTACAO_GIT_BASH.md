# Preparando a publicação com Git Bash

1. Preserve **cópia intacta** da pasta original.
2. Faça o inventário com Python, fora da pasta que vai ser publicada:

   ```bash
   python scripts/inventariar_arquivos.py "C:/CAMINHO/Modificacao TCC-HARDWORE - SOFTWARE"
   ```

3. Revise ZIPs e arquivos que possam conter chaves, dados pessoais ou material de terceiros.
4. Verifique se os históricos originais já são Git: entre em cada pasta e execute `git rev-parse --is-inside-work-tree`. Preserve `.git` existente, se houver.
5. Crie um repositório **privado inicialmente** em seu perfil no GitHub. Não atribua datas passadas aos novos commits.
6. Quando conteúdo seguro estiver organizado, abra Git Bash na raiz da **cópia**:

   ```bash
   git init -b main
   git status
   git add README.md .gitignore docs/ firmware/ software/ hardware/ ensaios/ artefatos/ scripts/
   git diff --cached --stat
   git diff --cached
   git commit -m "docs: inicia repositório com inventário histórico documentado"
   git remote add origin https://github.com/Dev-Cleyton/NOME_DO_REPOSITORIO.git
   git push -u origin main
   ```

   `NOME_DO_REPOSITORIO` é um exemplo: crie o repositório antes de executar o push e confira a URL correta.

7. Publique versões históricas em etapas, com uma documentação do que foi encontrado e testado, mantendo os commits de importação com datas reais. Gere tags ou releases por versão quando a origem do código estiver conferida.

8. Se uma versão usar código de terceiros, registre origem e licença. Não publique automaticamente datasets, `.env`, banco de clientes ou imagens privadas.

É mais defensável para a banca apresentar documentação com fontes datadas e histórico Git honesto do que retroagir artificialmente datas de commits.
