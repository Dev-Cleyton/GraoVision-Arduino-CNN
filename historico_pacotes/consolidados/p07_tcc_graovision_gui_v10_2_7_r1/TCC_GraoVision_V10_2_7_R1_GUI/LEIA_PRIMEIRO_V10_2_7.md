# V10.2.7 — segundo percurso diagnóstico P4 e P5

Base integral da V10.2.6, sem alterações de calibração, CNN ou equações de umidade.

Fluxo: tara -> P1 -> CNN -> primeira subida/P2 -> monitoramento/PF -> primeira descida/P3 -> solicitação presencial -> segunda subida/P4 -> segunda descida/P5 -> FIM:1.

**IMPORTANTE:** depois de P3, o Arduino aguarda `DIAG:AUTORIZAR` ou `DIAG:CANCELAR`. O Python exige `AUTORIZAR-P4-P5` no console. Sem autorização, `DIAG:CANCELAR` pula as medições extras e finaliza na base. Os 28.000 pulsos adicionais representam mais um percurso inteiro. A segunda subida nao deve ocorrer automaticamente.

**Seguranca:** aquecedor D7 continua bloqueado nas duas compilações. Nunca energize a lampada; homing físico ainda ausente. Valide primeiro compilação, simulação e ensaio sem carga/potência e com parada de emergencia fisica independente. EEPROM continua como posicao COMANDADA, não referência absoluta. Se falhar com eixo elevado, não movimente sem inspeção presencial.

**Dados:** 12 leituras em cada vetor. P4 e P5 usam a mesma tara; sem nova CNN nem aquecimento, sem remoção ou adição de graos. Relatórios preservam CSV/JSON anteriores e acrescentam P4/P5, P4-P2, P5-P3 e P5-P1. Comparações descrevem diferenças aparentes, não exatidão nem teor de umidade.

**Instalacao:** abra `.ino` da pasta `V10_2_5_Controle_Termico_Simulacao` para ensaios simulados; a pasta `V10_2_5_Controle_Termico_Real` requer reinspeção mecânica. Copie o arquivo `.env` de sua instalação anterior apenas depois de revisar os armamentos. Não copie credenciais para repositórios públicos.
