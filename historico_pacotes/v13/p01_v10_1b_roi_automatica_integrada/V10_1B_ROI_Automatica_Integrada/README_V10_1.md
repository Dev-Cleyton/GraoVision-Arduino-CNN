# TCC Analise de Graos — V10.1, primeira integracao (VALIDACAO)

**Data:** 07/11/2026. **Base:** arquivos do usuario em `Dezima Versao_10.zip` e decisoes da auditoria historica. O firmware V10 arquivado foi preservado em `referencias/ORIGINAL_V10_NAO_CARREGAR_SEM_REVISAO.ino`.

## 0. Escopo e limites — LEIA PRIMEIRO

- **AQUECEDOR BLOQUEADO NO CODIGO:** o rele D7 nunca recebe comando LOW nesta versao. **Nenhum contato COM/NO/NC deve ter aquecedor ou outras cargas de potencia conectadas.** Os relés ativos em LOW podem oscilar na energização/reset do Uno mesmo quando o código comanda HIGH.
- **MOVIMENTOS FISICOS DESABILITADOS por padrao** (`HABILITAR_MOVIMENTOS_REAIS = false` no topo do `.ino`). Com essa configuração, as mensagens de movimento são simuladas e o peso P2 é apenas a segunda pesagem física; a plataforma não muda de posição. Não usar P2 assim obtido para concluir que a movimentacao real nao altera o peso.
- O firmware contém funções de movimento herdadas da V10 com angulos 0/90/180 e 14.000 passos, mas a maquina **nao tem homing/fins de curso comprovados no conjunto de arquivos auditados**. **Nao habilitar movimento real** ate revisar percursos, topes, seguranca e posicao conhecida. Rele D6 de alimentacao do driver permanece OFF por padrao.
- NTC A2 está perto da célula de carga. Não controla aquecedor, não mede temperatura dos grãos nem porcentagem de água dos grãos. Para aquecimento real, serão exigidos sensor apropriado na região de secagem, protecao termica independente, validacao dos contatos e circuito de potencia.
- Fator `+1058.3167f` é **PROVISÓRIO** (calibracao com massa nao certificada). Na capacidade de 1 kg, a repetibilidade com ~36 g não certifica exatidão em toda a faixa.
- **Sem sensor/homologacao de umidade:** a CNN pode ser aprovada, mas o status final permanece `PENDENTE_VALIDACAO_UMIDADE`; **perda de massa nao e umidade absoluta**. A opcao de acrescentar laudo externo e separada.
- **Teste de validacao**, nao firmware de producao. Primeira passagem na Arduino IDE e testes fisicos controlados ainda sao obrigatorios.

## 1. Arquivos entregues

```
V10_1_Integrada_Validacao/
  arduino/codigo_automacaoV10_1_Integrada/codigo_automacaoV10_1_Integrada.ino
  analise_graos/
    main.py
    aplicar_umidade_validada.py
    .env.example
    requirements.txt
    core/analysis_service.py
    core/grain_classifier.py
    core/sample_service.py
    hardware/arduino_handler.py
    hardware/camera_handler.py
    models/sample.py
    models/analysis_session.py
    models/analysis_result.py
    infrastructure/csv_service.py
    utils/constants.py
    tests/test_protocolo_v101.py
  referencias/ORIGINAL_V10_NAO_CARREGAR_SEM_REVISAO.ino
  README_V10_1.md
  PROTOCOLO_SERIAL_V10_1.md
  TESTES_E_LIMITACOES.md
```

A camera, a rotina de inferência com Roboflow kernel-corn v54 e ROI circular provêm da V10 enviada, com pequenas melhorias: limite de espera de estabilização da camera e registro da quantidade de cada classe aceita na ROI. Nao foram incluidos os pesos de rede, credenciais, pastas de imagens pessoais, `.env` verdadeiro ou ambiente virtual Windows.

## 2. Pinagem adotada

| Recurso | Arduino Uno | Acionamento |
|---|---|---|
| HX711 SCK/DOUT | A0 / A1 | calibracao positiva provisoria |
| NTC 10k / divisor 10k | A2 | NTC ligado a GND, resistor ao 5V |
| Servo suporte | D9 | 90 centro, 0 camera, 180 secagem **a confirmar mecanicamente** |
| TB6600 DIR/STEP | D2 / D3 | HIGH subir; 14.000 passos de referencia |
| Rele1: aquecedor | D7 | HIGH = OFF; nao ligamos no firmware |
| Rele2: driver | D6 | HIGH = OFF no modo padrao |
| Rele3 / Rele4 | D8 / D4 | HIGH = OFF |

Nunca usar o pin D11 para controle do aquecedor neste hardware; a V10 antiga diverge da montagem atual.

## 3. Instalar e executar (Windows + Arduino IDE + Python 3.13)

1. Mantenha sua pasta `Dezima Versao_10` como backup, sem sobrescrever nada.
2. Desligue Arduino e fontes externas. Desconecte TODAS as cargas de potencia dos contatos COM/NO/NC; confira a pinagem antes de conectar o USB.
3. Abra `arduino/codigo_automacaoV10_1_Integrada/codigo_automacaoV10_1_Integrada.ino` na Arduino IDE. Selecione **Arduino Uno**, a porta adequada e a biblioteca **HX711** compatível com a API `begin`, `set_scale`, `tare`, `get_units`, `wait_ready_timeout`; a Servo é padrão. Clique **Verificar**. (Ainda não compilado com as bibliotecas reais nesta entrega.)
4. Observe no topo do `.ino`: `HABILITAR_MOVIMENTOS_REAIS = false` e nenhum comando para ligar o aquecedor. Carregue o firmware somente com **cargas de potência desconectadas**.
5. Conecte Arduino USB ao PC. Feche o Monitor Serial da IDE antes de abrir o Python, pois a porta COM só aceita um controlador por vez.
6. No terminal, entre em `analise_graos` e execute:

```
py -3.13 -m venv .venv
.venv\Scripts\activate
python -m pip install -r requirements.txt
copy .env.example .env
```

7. Edite o `.env` **local** colocando a sua chave Roboflow. Nao envie esse arquivo a terceiros nem o coloque em ZIP. Por padrao o Python usa a porta **COM4**; se necessario, ajuste `port='COM4'` em `hardware/arduino_handler.py` (veja a porta na Arduino IDE).
8. Com a plataforma em posicao inferior e centralizada, prato vazio, sem cargas de potencia, execute `python main.py`. Confirme tara e depois coloque a amostra quando solicitado. **No modo simulado o servo nao gira**: a camera só capturara corretamente se a webcam já estiver apontada para a amostra. Caso contrário, faça primeiro somente os testes de protocolo simulados (`python -m unittest discover -s tests -v`).
9. Em cada ciclo, observe o status no terminal. Arquivos gerados em `analise_graos/relatorios` com identificador da amostra. Para repetir o ciclo, remova a amostra anterior e faça nova tara vazia.

### Testar software sem Arduino e sem API Roboflow

Dentro de `analise_graos`, execute:

```
python -m unittest discover -s tests -v
```

Os testes falsificam a serial e a classificação só para checar ordenacao de mensagens/estados. Seus resultados não são ensaios físicos ou deteccoes reais.

## 4. Fluxo efetivamente implementado

- Operador confirma inicio central/inferior e o prato vazio. `TARA` uma vez por ciclo; sem EEPROM.
- Operador coloca amostra e envia START: UNO mede e envia `P1` antes de mover/camera.
- Arduino anuncia `CMD:ACTIVATE_CAMERA`; Python captura **uma imagem**, executa CNN Roboflow e retorna status.
- `CAM:RUIM`: **não eleva nem aquece**, retorna logicamente ao centro, envia `FIM:REPROVADA_CNN`, gera relatório com contagens/percentual real da CNN.
- Nenhuma detecção: `CAM:INCONCLUSIVA`; relatório sem afirmar que são grãos ruins.
- CNN aprovada: retorna logicamente ao centro, sobe, gira para a area de secagem, estabiliza 8 segundos, mede `P2`, acompanha massa a cada 30 s por 2 minutos **SEM CALOR**, mede `PF`, retorna ao centro e desce (movimentos simulados por padrão).
- Python armazena historico e calcula `delta_posicionamento = P2-P1` e `perda de massa = P2-PF` como grandezas **distintas**.
- Status final: `PENDENTE_VALIDACAO_UMIDADE` sem método confiável de teor de água; nova tara no ciclo seguinte. Leituras depois da retirada nao entram no relatorio.

## 5. Relatorio de reprovação por umidade

A V10.1 **não inventa umidade** a partir da temperatura nem da massa perdida. Quando houver um valor obtido por laboratório ou metodo gravimétrico validado e o limite empresarial fundamentado, é possivel registrar esse dado depois, em um **arquivo complementar**, sem alterar o original:

```
python aplicar_umidade_validada.py relatorios/ID_relatorio.json --umidade 16.2 --limite 14.0 --metodo "Metodo laboratorial validado" --referencia "Laudo identificado"
```

O script só aceita `CNN APROVADA` com status `PENDENTE_VALIDACAO_UMIDADE`, exige identificador da referência e cria `*_UMIDADE_VALIDADA.json`. Resultado é `REPROVADA_UMIDADE` se `U > limite`, caso contrário `APROVADA`. **Nunca registre valores fictícios como laudo.** O limite e o método devem ser definidos para o tipo de grão e empresa.

## 6. Proximos passos sem repetir testes anteriores

1. Compilar o Arduino na IDE real e comparar mensagens com `PROTOCOLO_SERIAL_V10_1.md`.
2. Testar a comunicacao integrada (sem potencia e com movimentos simulados) e confirmar que os relatórios são gravados.
3. Verificar se o conjunto fisico possui referencial de posicao/limites de curso adequados antes de habilitar os movimentos.
4. Validar P1 e P2 na montagem **efetivamente movida**, preservando os ensaios de servo/stepper que ja foram realizados no TCC. Confirmar se existe diferenca sistematica relevante; nao assumir resultado pela simulacao.
5. Planejar controle e proteção térmica real e validar um método de teor de umidade conforme o produto e a especificação empresarial. Apenas depois discutir aquecimento real e aprovacão automática final.
