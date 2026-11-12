# GrãoVision

**Classificação de grãos por visão computacional e automação de uma bancada experimental**

Projeto de Trabalho de Conclusão de Curso (TCC) em Engenharia de Computação. O GrãoVision reúne **Arduino**, **Python**, **sensores de pesagem e temperatura** e **visão computacional com IA** para estudar a avaliação de amostras de grãos em uma bancada automatizada.

> **Início informado do projeto:** 16 de julho de 2025. O histórico de desenvolvimento foi organizado retrospectivamente em Git em 2026, a partir de versões e pacotes locais preservados. As datas dos commits de organização não representam necessariamente as datas em que cada código foi originalmente escrito.

## Objetivo

Investigar a integração entre medição física e análise visual de amostras: pesagem, aquisição de imagens, classificação de grãos, processo térmico supervisionado e consolidação dos resultados. Trata-se de um **protótipo acadêmico/experimental**, não de um equipamento industrial certificado ou pronto para operação sem avaliação.

## Visão geral da arquitetura

```text
                  Computador / Python
                ┌──────────────────────┐
                │ Coordenação do fluxo │
                │ Câmera e imagens     │
                │ Classificação por IA │
                │ Registros/relatórios │
                └──────────┬───────────┘
                           │ USB / Serial
                ┌──────────▼───────────┐
                │      Arduino Uno     │
                │ Comandos e sensores  │
                └──────────┬───────────┘
                           │
                 ┌─────────┴─────────┐
                 │ HX711 / pesagem   │
                 │ NTC / temperatura │
                 │ Servo / motor     │
                 │ Relés / controle  │
                 └───────────────────┘
```

Nas versões do projeto, a arquitetura e a pinagem podem variar. Consulte o firmware e a documentação do pacote específico antes de conectar componentes.

## Tecnologias e competências envolvidas

| Área | Recursos empregados ou investigados |
|---|---|
| Sistemas embarcados | Arduino Uno, firmware em C/C++ para Arduino, comunicação serial |
| Instrumentação | Célula de carga com HX711, termistor NTC, ensaios de deriva e calibração |
| Automação | Servo, motor de passo, acionamento por relés e rotinas supervisionadas |
| Visão computacional | Webcam, captura de imagens, região de interesse (ROI) e classificação de grãos |
| Inteligência artificial | Inferência com projeto de classificação/detecção de grãos no Roboflow (histórico `kernel-corn`) |
| Software | Python, organização em módulos, tratamento de erros, registros e relatórios |
| Engenharia de software | Versionamento Git, rastreabilidade de versões, documentação de experimentos |

## Fluxo de análise estudado

O fluxo **previsto ou implementado parcialmente conforme a versão** inclui:

1. Inicialização da bancada, verificação de comunicação e tara da balança.
2. Obtenção do peso inicial da amostra.
3. Posicionamento para captura de imagens por webcam.
4. Análise visual por IA e resposta ao controlador.
5. Posicionamento para etapa térmica, quando disponível na versão e no ensaio.
6. Nova pesagem e análise da variação de massa.
7. Consolidação dos dados e encerramento supervisionado.

**Importante:** nem todas as versões possuem todas essas etapas, e a existência de uma rotina no código não comprova sua validação experimental.

## Evolução do desenvolvimento

| Família histórica | Conteúdo registrado |
|---|---|
| V01–V09 | Evolução do firmware, módulos Python, comunicação com câmera e rotinas de análise; inclui variantes e correções preservadas |
| V10 | Pacotes e ensaios específicos com HX711, pesagem, servo, motor de passo, relés, NTC e diagnósticos de estabilidade |
| V10.1 / V10.1A / V10.1B / V10.1C | Integração, experimentos de ROI, captura em múltiplos ciclos e revisões de movimento supervisionado |
| V10.2 e consolidados | Pacotes de controle térmico, compensação de massa, supervisão, EEPROM, interfaces gráficas e rastreabilidade |

Os nomes acima identificam **famílias de arquivos históricos** e não constituem uma declaração de que todas as funcionalidades foram aprovadas em testes. Revisões do mesmo estágio e códigos de teste são mantidos de modo separado.

## Organização do repositório

```text
.
├── firmware/historico/       # Firmwares das versões iniciais
├── software/historico/       # Código Python das versões iniciais
├── historico_pacotes/        # Fontes selecionadas dos ZIPs posteriores
├── docs/historico/           # Relatos e manifestos de rastreabilidade
└── README.md                 # Apresentação do projeto
```

Os arquivos originais em ZIP foram preservados localmente durante a organização, mas **não são distribuídos integralmente neste repositório**. Os manifestos relacionam os códigos selecionados e seus pacotes de origem. Ambientes virtuais, arquivos `.env` e outros itens não necessários foram excluídos da importação.

## Como explorar o código

Este repositório é um **acervo de versões históricas**, e não um único aplicativo pronto para instalar. Para analisar ou reproduzir uma versão:

1. Escolha o pacote e leia sua documentação e seu manifesto.
2. Verifique os requisitos Python e as bibliotecas Arduino daquela versão.
3. Revise a pinagem, o protocolo serial e os limites de segurança.
4. Configure credenciais necessárias por variáveis de ambiente locais; **nunca** as envie ao Git.
5. Valide componentes isoladamente antes de executar movimentos ou acionamentos de potência.

Não existe, neste momento, um comando único garantido para executar todas as versões.

## Segurança e limitações

- **Não energize aquecedores, motores ou cargas de potência apenas com base em código histórico.** Faça revisão elétrica, intertravamentos e ensaios supervisionados.
- Alguns módulos de relé são **ativos em nível baixo**; estados durante inicialização ou reset exigem atenção especial.
- A medição de temperatura por um sensor local não substitui, por si só, a proteção independente da câmara térmica.
- Testes de software, medições e diagnóstico de sensores devem ser interpretados no contexto da versão específica; não há validação universal de todos os pacotes.
- Credenciais e dados pessoais não devem ser colocados em exemplos ou issues públicas.

## Autoria e finalidade

Projeto acadêmico de **Cleyton Junior dos Santos**, desenvolvido como parte da formação em Engenharia de Computação. Este repositório também serve como portfólio técnico de programação, instrumentação, automação e visão computacional.

**Licenciamento:** ainda não foi definida uma licença de reutilização para o conjunto do repositório. A disponibilização do código para consulta não implica autorização automática para redistribuir conteúdo de terceiros. Antes de reutilizar componentes, confira as respectivas licenças e atribuições.

---

**Status:** histórico técnico organizado para documentação e avaliação. Funcionalidades e resultados devem ser confirmados nos artefatos e ensaios correspondentes.
