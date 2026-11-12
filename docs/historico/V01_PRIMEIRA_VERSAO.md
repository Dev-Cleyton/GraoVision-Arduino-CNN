# V01 — Primeira versão histórica do GrãoVision

## 1. Proveniência e finalidade

Importação retrospectiva do arquivo `Primeira Versao_1/TCC -HARDWRE -SOFTWARE.zip`, preservado separadamente do repositório Git. Esta documentação descreve o conteúdo encontrado no ZIP; não constitui comprovação de execução física ou validação experimental.

**Data histórica do desenvolvimento/testes:** não comprovada apenas pelo ZIP. Os horários e datas registrados nos arquivos ZIP são metadados, não evidência suficiente da data dos ensaios. O commit de importação deverá usar sua data real de registro no Git, sem retrodatação.

**Estado:** protótipo acadêmico experimental, não certificado. O cálculo apresentado como umidade é uma *estimativa* baseada em temperatura e pressão de vapor assumida; não é uma medição validada de teor de umidade do grão.

## 2. Arquivos de código preservados

- **Firmware (2 sketches):** `codigo_automacao.ino` e `codigo_automacaoV2.ino`, copiados para `firmware/historico/v01_primeira_versao/`.
- **Software (16 arquivos `.py`):** módulos `core/`, `hardware/`, `infrastructure/`, `models/`, `tests/`, `utils/` e o ponto de entrada `main.py`, em `software/historico/v01_primeira_versao/analise_graos/`.
- **Dependências declaradas:** `opencv-python`, `pyserial`, `numpy`, `roboflow` (`requirements.txt`). O código também importa `dotenv`, mas `python-dotenv` não aparece no `requirements.txt` original.
- **Módulos vazios no original:** `infrastructure/csv_service.py`, `infrastructure/database_service.py` e `infrastructure/logger_service.py` têm tamanho zero no ZIP. Foram preservados assim para fidelidade histórica.

Não foram importados os milhares de arquivos do ambiente virtual (`venv/`), o `.env` com credencial nem as fotografias incluídas no ZIP.

## 3. Arquitetura observada no código

O Python coordena a comunicação serial com o Arduino, a captura de imagens pela webcam e a classificação visual de grãos usando Roboflow. O Arduino controla estados do processo, servo, motor de passo, pesagem com HX711 e leitura de NTC.

### Firmware

- Bibliotecas: `EEPROM`, `Servo`, `HX711`; a revisão V2 inclui `math.h`.
- Pinos presentes no código V2: HX711 `SCK=A0`, `DT=A1`; motor de passo `DIR=2`, `STEP=3`; relé de temperatura `D6`; servo `D9`; `heaterPin=D11`; NTC `A2`.
- Contém uma máquina de estados incluindo captura de câmera, pesagem, movimentação, leitura ambiental e encerramento.
- Mensagens do protocolo incluem `READY: Awaiting START command`, `START`, `CMD:ACTIVATE_CAMERA`, `CAM:SUCESSO`/`CAM:RUIM`, `PESO:`, `PESO2:` e `FIM:1`.
- O posicionamento do atuador de passo e o fator de calibração são lidos da EEPROM; o código também registra posição do atuador.

### Python

- `main.py`: inicia sessões e apresenta os resultados no terminal.
- `core/analysis_service.py`: orquestra a sessão, comunicação Arduino e processamento da amostra.
- `core/sample_service.py`: captura imagens, executa o classificador e determina uma regra nominal de reprovação acima de 25% de grãos ruins.
- `core/grain_classifier.py`: usa o projeto Roboflow `kernel-corn`, versão `54`; solicita predições com `confidence=28` e `overlap=24` e contabiliza a classe `bad`.
- `hardware/camera_handler.py`: usa OpenCV/DirectShow com índice 0, resolução solicitada de 2560×1440, descarte inicial de quadros e verificação de brilho antes da captura.
- `hardware/arduino_handler.py`: usa comunicação serial em `COM4`, 9600 baud, e interpreta respostas do Arduino.
- `tests/teste_camera.py`: contém um teste manual de abertura da webcam, **não um relatório de teste aprovado**.

## 4. Limitações e inconsistências encontradas

1. **Aprovação forçada:** em `analysis_service.py`, o método `iniciar_analise()` atribui `sample.aprovado = True` antes de enviar o resultado ao Arduino. Isso impede que a decisão visual determine corretamente a aprovação nesse fluxo.
2. **Nomes de atributos divergentes:** `models/sample.py` declara `aprovado`, mas `sample_service.py` atribui `aprovada`. São atributos diferentes em Python. A inconsistência é relevante para o registro do resultado e reforça a necessidade de não considerar a lógica como validada.
3. **Formato serial incompatível entre revisões:** a revisão `codigo_automacaoV2.ino` emite `TEMPERATURA:` e `UMIDADE:` separadamente, enquanto `arduino_handler.py` procura uma mensagem iniciada por `Temperatura:` contendo `Umidade Estimada:` separada por `|`. Isso pode impedir a coleta correta de dados ambientais ao combinar essa revisão com esse receptor.
4. **Dependência ausente no manifesto:** `python-dotenv` é utilizada nos imports, mas não está listada no `requirements.txt` desta versão.
5. **Ausência de validação comprovada:** o ZIP contém código e um teste manual de webcam; por si só, não comprova funcionamento integrado, precisão metrológica, desempenho da classificação, teor de umidade ou segurança térmica.

## 5. Segurança e fidelidade histórica

- A cópia publicada **não deve** conter o `.env` original, chaves de API, dados privados, `venv/` ou imagens sem autorização de publicação.
- **Adequação de segurança documentada na cópia importada:** `main.py`, linha 10 da cópia, teve a impressão do valor de `os.getenv(...)` substituída por `print("Variavel de ambiente: valor ocultado por seguranca")`. O ZIP original foi preservado. A sintaxe da cópia do `main.py` foi verificada com `python -m py_compile`; isso **não** comprova execução ou integração.
- Como uma chave de API constava no ZIP original, recomenda-se revogá-la e substituí-la se ainda estiver ativa. A ausência de resultados em buscas por padrões conhecidos de chaves **não garante** ausência total de segredos; a inspeção final dos arquivos deve ocorrer antes da publicação.
- **Não executar o firmware com atuadores, relés ou aquecedor energizados sem revisão da montagem e intertravamentos.** A polaridade de acionamento dos relés precisa ser confirmada no hardware físico; estados `LOW`/`HIGH` na inicialização podem ser críticos dependendo do módulo.
- Mudanças funcionais futuras devem ser registradas em versões posteriores, sem alterar silenciosamente o comportamento histórico da V01.

## 6. Evidências e pendências

**Confirmado pela inspeção do ZIP:** nomes e estrutura dos arquivos, código dos dois sketches, módulos Python e os três arquivos de infraestrutura originalmente vazios.

**Confirmado pela verificação manual da cópia feita durante a importação:** presença dos 19 arquivos previstos; proteção da impressão da variável no `main.py`; sucesso da compilação sintática desse arquivo.

**Pendente:** inspeção final de credenciais; decisão sobre revogação da chave do Roboflow; testes de execução sem cargas; registro de logs e fotos autorizadas; verificação da compatibilidade entre revisões e do comportamento mecânico/térmico.

## 7. Reprodutibilidade

Esta V01 foi mantida para rastreabilidade acadêmica. O `requirements.txt` é histórico e não assegura instalação reproduzível sem ajustes. A execução exige dependências e hardware específicos, uma chave própria de API fornecida por variável de ambiente e revisão prévia de segurança. Não há garantia de funcionamento integral nesta revisão.
