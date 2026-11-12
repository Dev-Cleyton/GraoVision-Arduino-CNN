# GrãoVision 1.0 — Interface desktop para o TCC (V10.2.7 R1)

## O que está entregue

Dashboard profissional em Tkinter com painéis de temperatura, P1/P2/PF/P3/P4/P5, etapas, telemetria serial, histórico de alarmes e visualização da webcam durante a captura. Após a inferência Roboflow, mostra a imagem anotada (incluindo ROI e caixas). O vídeo **é prévia em tempo real durante a captura**, não inferência quadro a quadro.

Controles: iniciar ciclo, STOP por software, solicitações gráficas de ARMAR, tara, confirmação de referência X (EEPROM), confirmação de retorno após resfriamento, autorização P4/P5, e **retorno supervisionado somente se o próprio firmware o disponibilizar**. Mensagens de falha contêm causa e orientação. Configurações de COM, índice da webcam, movimentos reais e ROI são protegidas por PIN administrativo local, definido no primeiro acesso. O PIN não é salvo em texto puro e não altera as proteções do Arduino.

O firmware e o `main.py` de terminal foram preservados; os ajustes no backend Python são apenas ganchos de eventos/preview/prompt para a GUI e verificação de ACK de confirmação térmica. **Não foi validado fisicamente nesta compilação.**

## Instalação Windows 10 + Python 3.13

1. Descompacte o ZIP mantendo a estrutura de pastas.
2. Copie o seu `.env` antigo (contendo `ROBOFLOW_API_KEY`) para `analise_graos\.env` da nova pasta. Não envie a chave a outras pessoas. Se não tiver, copie `.env.example` para `.env` e configure sua própria chave.
3. Execute `INSTALAR_DEPENDENCIAS.bat` (precisa de internet para `pip`).
4. Execute `INICIAR_GRAOVISION.bat`.
5. Abra **Configurações**, crie seu PIN administrador local no primeiro acesso, configure a porta COM real (geralmente COM4) e o índice da webcam (normalmente 0). Habilite movimentos reais apenas se as condições físicas estiverem confirmadas; a GUI ainda pede ARMAR presencial por ciclo.
6. Retorne à Visão geral e clique em **INICIAR ANÁLISE**. Acompanhe as solicitações gráficas e a telemetria.

Alternativa PowerShell, na pasta `analise_graos`:

```powershell
& "C:\Program Files\Python313\python.exe" -m pip install -r requirements.txt
& "C:\Program Files\Python313\python.exe" iniciar_interface.py
```

O sistema salva os resultados em `analise_graos\relatorios` e as capturas em `analise_graos\imagens`, como antes.

## Segurança e o botão Retorno

- **Não existe homing**: EEPROM grava a última posição/ângulo *comandados*, não a posição física confirmada. O botão **CONFIRMAR REFERÊNCIA X** somente responde à solicitação do firmware quando o atuador estiver realmente embaixo e o servo no centro, com prato vazio. Nunca use esse comando com o conjunto no alto.
- **RETORNO SUPERVISIONADO** só fica habilitado quando o firmware emite `RECUPERACAO:RETORNO_SUPERVISIONADO_DISPONIVEL` na sessão corrente. Uma falha térmica pode bloquear o retorno — a GUI nunca força essa trava nem energiza o motor à revelia do firmware.
- Durante resfriamento, a GUI apresenta `RETORNO-RESFRIADO` para inspeção presencial. A confirmação é enviada como `CONFIRMO:RESFRIAMENTO`; **não é considerada aceita sem o ACK `RESFRIAMENTO:CONFIRMACAO_PRESENCIAL_RECEBIDA`**. Se o firmware recusar ou expirar o prazo, não haverá retorno.
- A versão gráfica mantém **`AQUECIMENTO_REAL=0`** por decisão de segurança, mesmo se o `.env` antigo contiver `AQUECIMENTO_REAL=1`. **Não é a interface de aquecimento real validado.** O firmware incluído mantém o aquecedor bloqueado. Não desbloqueie o calor no `.ino` para testar a GUI. A sequência sem calor é a referência para ensaio do HMI.
- `STOP` é apenas comando serial por software. Não substitui o dispositivo físico de emergência, os intertravamentos e o circuito de corte térmico independente. Nunca desconecte o driver de um eixo vertical sem conferir a retenção mecânica.
- Credenciais da GUI são controles locais de acesso, não segurança de máquina, autenticação do Arduino ou certificação industrial.

## Testes disponíveis

Execute da pasta `analise_graos`:

```powershell
& "C:\Program Files\Python313\python.exe" -m pytest -q
```

Testes automáticos Python podem passar sem hardware; abertura da janela não comprova funcionamento físico. Validar em bancada com o aquecedor **fisicamente desconectado** e supervisão presencial. Para executar os testes, instale `pytest` caso ainda não esteja instalado.

## Notas de imagem

Câmera é aberta somente após `CMD:ACTIVATE_CAMERA`. Enquanto captura, são exibidos frames JPEG reduzidos para manter a interface fluida; a resolução original é preservada para o classificador. Depois da inferência, a imagem `*_detectada.jpg` é exibida no painel. Não executamos novas inferências em cada frame.

## Componentes

- `analise_graos/iniciar_interface.py` — nova entrada GUI
- `analise_graos/interface/dashboard.py` — janela e visualização
- `analise_graos/interface/operacao.py` — prompts, configurações, senha local e orientações
- `analise_graos/main.py` — entrada original do terminal
- `analise_graos/hardware/arduino_handler.py` — eventos e prompts injetáveis, sem novos comandos inseguros
- `analise_graos/hardware/camera_handler.py` — opção de quadros de prévia
- `arduino/...` — sketches herdados da R1 FIX, sem alterações

## Novidade: 3 aparências selecionáveis

Acesse **Aparência • 3 estilos** no menu esquerdo para escolher **Esmeralda**, **Fluent Azul** ou **Operador**. A troca não exige PIN e é lembrada no próximo uso. Consulte `LEIA_TRES_TEMAS.md`.
