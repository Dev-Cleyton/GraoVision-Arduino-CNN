# Alteracoes V10.1B versus V10.1

- `analise_graos/core/roi_automatica.py`: detector V10.1A V2 integrado.
- `analise_graos/core/grain_classifier.py`: ROI automatica principal;
  predicoes da foto original; visualizacao e comparacao fixa diagnostica.
- `analise_graos/models/sample.py`: estados e historico da ROI.
- `analise_graos/infrastructure/csv_service.py`: ROI nos JSONs de relatorio.
- `analise_graos/core/analysis_service.py`: evita enviar STOP duas vezes
  quando ocorre erro de camera/IA/ROI.
- `analise_graos/.env.example`: flag opcional `ROI_AUTOMATICA=1`.
- `analise_graos/validar_roi_offline.py`: teste rapido da geometria.
- `analise_graos/tests/test_roi_integrada_v101b.py`: testes automatizados offline.
- `arduino/...ino`: **arquivo identico ao da V10.1 original**.

**Nao alterados:** pesos/configuracao Roboflow kernel-corn V54, threshold 28,
overlap 24, regra bad>25%, sistema de pesagem, movimento, calor bloqueado,
protocolo serial, sensor NTC.
