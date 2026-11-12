# Python da V10.1C-R2

Projeto da V10.1C com ROI automática e câmera multiciclos. `CENTRALIZAR` é enviado antes de `TARA`. Telemetria `PASSOS` vai para o PowerShell, JSON e CSV de movimentos.

Edite `.env` para combinar com o sketch do Arduino: `MOVIMENTOS_REAIS=0` para `V10_1C_R2_Simulacao_Segura`; `MOVIMENTOS_REAIS=1` para `V10_1C_R2_Movimento_Real_Supervisionado`, somente após assegurar proteções físicas. Aquececimento real bloqueado.

Execute a partir desta pasta com `python main.py`.
