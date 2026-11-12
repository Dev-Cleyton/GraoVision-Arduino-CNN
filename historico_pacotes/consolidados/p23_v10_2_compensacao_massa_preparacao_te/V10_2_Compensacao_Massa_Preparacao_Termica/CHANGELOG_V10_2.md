# Alteracoes em relacao a V10.1C-R3
- Firmware Arduino binariamente-fonte identico; NAO regravar para calculo de massa.
- Novo modulo Python `core/balanco_massa.py` com offset, PF corrigido, percentuais.
- `models/sample.py` preserva as leituras originais e agrega diagnostico de balanco.
- JSON e CSV adicional de balanco de massa por ciclo.
- CLI offline `simular_secagem_offline.py`.
- Testes unitarios extras para offset positivo/negativo, leitura crescente, falta P2,
  preservacao de leituras e verificacao de relatorios.
- Bloqueio de aquecimento e protecoes R3 sem alteracoes.
