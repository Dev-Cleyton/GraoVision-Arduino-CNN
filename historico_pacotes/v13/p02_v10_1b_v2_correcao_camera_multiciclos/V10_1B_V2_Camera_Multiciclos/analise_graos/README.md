# Aplicacao Python V10.1B — ROI AUTOMATICA INTEGRADA

Leia `../README_V10_1B.md` antes de usar. A chave Roboflow deve ficar somente no `.env` local, nunca neste ZIP.

Modo padrao: `ROI_AUTOMATICA=1`. Se falhar a localizacao da bandeja, o sistema encerra como ERRO_PROCESSO sem CAM:RUIM/CAM:SUCESSO.

Testes offline: `python -m unittest discover -s tests -v` (na pasta `analise_graos`).

Execucao integrada: `python main.py`, com cargas de potencia desconectadas e movimentos simulados no firmware.

Os arquivos gerados sao salvos relativos ao diretorio atual do terminal: `imagens/ID/` e `relatorios/`.
