# GrãoVision — gráficos detalhados e estudo 2N2907 (11/11/2026)

## O que foi integrado na GUI (sem mudar o firmware que já está instalado)
- Nova entrada no menu: **Gráficos detalhados** (nos três temas).
- Temperatura NTC A2: somente pontos realmente transmitidos (`MEDICAO`, opcional `TEMP_MONITOR`), tooltip no mouse e eixo em segundos do monitoramento.
- Massa HX711: 12 leituras de cada P1, P2, PF, P3, P4, P5, dados `MEDICAO` durante os 120 s e alinhamento temporal aproximado ao último vetor P2, usando milissegundos do firmware (não inferir tempo de recepção do computador).
- Botões **EXPORTAR CSV** (dados reais) e **EXPORTAR PNG** (figura dos gráficos).
- Preserva a tela de Testes Instrumentais, a CNN, o cadastro, os relatórios, a memória EEPROM, os três temas e o bloqueio de aquecimento.
- Sem aquecimento, a leitura do NTC e da massa de P2 a PF representa monitoramento/deriva, NÃO determina umidade.

## Por que ainda havia só cinco pontos de temperatura?
O firmware atual transmite `MEDICAO` a cada 30 s, além do ponto final (121 s aprox.). Uma interface não cria medições reais que não recebeu. Para obter mais pontos de temperatura é preciso que o firmware efetivamente os envie.

## Opcional: firmware experimental NTC 5 segundos SEM H4
Na pasta `arduino_experimental/V10_2_7_NTC_5s_SEM_H4/` há um `.ino` opcional, derivado do firmware do pacote.
Acrescenta a mensagem `TEMP_MONITOR:segundos:temperaturaC` a cada cerca de 5 s, preservando o `MEDICAO` da balança de 30 s e preservando `AQUECEDOR_PERMITIDO_APOS_INSPECAO = false`.
**Não foi compilado nem testado fisicamente neste computador. Não grave por cima do firmware atual sem salvar backup e planejar teste mecânico seguro.** A tela já reconhece `TEMP_MONITOR` quando essa telemetria estiver disponível.
É possível usar o GrãoVision atualizado imediatamente com o firmware atual; apenas a densidade dos pontos de temperatura permanece limitada às mensagens atuais.

## 2N2907: estudo separado; NÃO controlar nem proteger a H4
Na pasta `diagnosticos/2n2907_bancada_sem_h4/` há um sketch de bancada independente, com resistor 10 kΩ, entrada A3 e transistor PNP ligado como diodo (base e coletor ao GND; emissor ao nó A3 e 10 kΩ para +5 V).
A junção apresenta variação aproximada de -2 mV/°C, mas a saída do sketch é **VEB em mV** e **variação relativa aproximada**, NÃO temperatura absoluta calibrada.

Pinagem depende do fabricante: por exemplo, P2N2907A/onsemi TO-92 é **C-B-E** e o 2N2907/Topdiode TO-92 pode ser **E-B-C**. **Não ligar sem identificar a peça e conferir o datasheet exato**.

O sketch usa `analogReference(INTERNAL)` (aproximadamente 1,1 V) e é **exclusivamente para bancada**, substituindo temporariamente o firmware de automação. NÃO é para integrar no firmware principal sem revisão: ali o NTC A2 usa referência AVcc. O circuito nunca recebe os 12 V da H4. A faixa térmica da junção ou embalagem **não é a faixa de medição calibrada nem um critério de segurança da câmara**. Para medir perto da lâmpada, preferir termopar K com condicionador e proteção térmica independente.

## Montagem e teste
1. Preserve o ZIP e o firmware atuais e os dados em `~/.graovision_tcc`.
2. Extraia este ZIP em uma nova pasta, replique seu `.env` (não incluso) e instale as dependências.
3. Inicie `INICIAR_GRAOVISION.bat`, abra **Gráficos detalhados** e execute o ciclo SEM H4 na aba **Testes instrumentais**.
4. Alterne para **Gráficos detalhados** para observar pontos recebidos em tempo real; ao terminar, exporte CSV e PNG.
5. Para estudar o transistor, use somente a montagem independente de bancada, após conferir sua pinagem e mantendo H4 desenergizada e desconectada. Não tente usar o sketch de bancada simultaneamente com o firmware principal.

O GrãoVision não é equipamento de medição de umidade certificado nem medidor térmico calibrado.
