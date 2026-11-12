# V10.1C — Plano de ensaio integrado de pré-produção

**Situação:** roteiro de validação, NÃO é uma autorização para aquecimento ou movimentação sem proteções físicas. Firmware V10.1B V2 permanece inalterado e, por padrão, simula os movimentos e bloqueia o aquecedor.

## Evidências já obtidas — não repetir sem razão técnica

- Comunicação UNO ↔ Python, tara, P1, captura, CNN, ROI automática, decisões `CAM:SUCESSO` / `CAM:RUIM`, P2/PF, CSV/JSON.
- Falha USB proposital da webcam: detectada, registrada como `ERRO_PROCESSO` e recuperação nos ciclos seguintes após reconexão. Não representa falha espontânea do dispositivo.
- Ensaios de HX711/servo/driver feitos isoladamente e comparados; novos testes devem se concentrar na operação integrada real e na repetibilidade.

## Porta 1 — Ensaio total a frio, com movimentos simulados

Situação: JÁ demonstrada na V10.1B V2; não precisa repetir apenas para confirmar o código.

1. Usar o projeto V10.1B V2 e `.env` local. Não compartilhar chave da API.
2. Manter aquecedor e cargas de potência fisicamente isolados, e `HABILITAR_MOVIMENTOS_REAIS=false`.
3. A cada ciclo iniciar com prato vazio, suporte inferior centralizado, tara; colocar amostra e iniciar.
4. Inspecionar `imagem_1_roi_visualizada.jpg` e `imagem_1_detectada.jpg` (não confiar apenas no texto `[OK]`).
5. Registrar resultado CNN, contagem manual da amostra, P1/P2/PF e divergências de ROI fixa vs automática.
6. Retirar grãos SOMENTE após o relatório e o fim do ciclo; para cada nova amostra tarar novamente.

**Esperado:** ramos de aprovação/reprovação seguros; falha da câmera/ROI não gera aprovação fictícia; `PENDENTE_VALIDACAO_UMIDADE` após monitoramento frio.

## Porta 2 — Teste mecânico supervisionado SEM aquecimento (AINDA NÃO LIBERADO)

Pré-requisitos obrigatórios antes de habilitar motor/servo no software:

- Batente/curso físico inspecionado, sem colisões nos ângulos realmente permitidos pelo conjunto. Não presumir que 0°, 90°, 180° são seguros.
- Referência de posição (homing validado ou confirmação independente de início de percurso) e proteção de **fim de curso superior e inferior**; controle do elevador não pode depender apenas de 14.000 pulsos.
- Botão de parada de emergência **físico**, capaz de retirar energia dos atuadores independentemente de Python/Arduino/USB.
- Proteção contra esmagamento, tombamento e derramamento; fonte elétrica e aterramento adequados, cabos fixados e nenhum operador dentro do percurso.
- Estratégia validada para falha de energia, reset, perda da COM, câmera/ROI indisponível e parada com mecanismo fora da posição inicial. Após falha NÃO reiniciar deslocamentos sem recuperação de posição.
- Teste gradual dos sentidos de giro, ângulos e limites sem grãos, depois com carga controlada, sob supervisão e sem potência térmica.

**Não basta alterar** `HABILITAR_MOVIMENTOS_REAIS` e `HABILITAR_RELE_DRIVER` para `true`: o firmware atual não confirma homing/limites, e o relé ativo em LOW pode pulsar durante reset.

Somente após essas barreiras, integrar em firmware revisado: centro inferior -> tara -> P1 -> câmera física -> CNN -> (rejeitado: centro e fim) OU (aprovado: centro -> sobe -> seca SEM calor -> P2 -> monitora -> PF -> centro -> desce -> fim). Não iniciar novo ciclo se a posição não foi confirmada.

## Porta 3 — Ensaio térmico / produção (AINDA NÃO LIBERADO)

Antes do aquecimento real:

- Sensor de temperatura calibrado instalado **na região térmica relevante da câmara**. O NTC A2 atual está ao lado da célula de carga e não protege a câmara.
- Corte térmico independente (termostato de segurança/limitador adequado e/ou fusível térmico), desligamento em sobretemperatura, falha de sensor e perda de comunicação; proteção permanece funcional com Arduino travado.
- Relé/contator/SSR e fiação dimensionados para potência real; separação adequada entre rede elétrica e circuitos de 5 V; inspeção elétrica competente. Não confiar no comando HIGH durante boot para evitar acionamento de relé LOW-ativo.
- Proteção de sobrecorrente/incêndio, isolamento térmico, distância da célula/servo, verificação de temperaturas e tempo.
- Método **validado de teor de umidade** com referência laboratorial/gravimétrica e limite adotado pela empresa. Variação de massa P2–PF não é, por si, umidade absoluta.
- Plano de rejeição/aborto seguro e verificação de repetibilidade metrológica do processo completo.

## Registros por ciclo

- ID da amostra e tipo (referência conhecida visualmente, amostra de rotina etc.).
- P1/P2/PF, peso ao longo do tempo, temperatura local (NTC) **sem confundi-la com temperatura da câmara**.
- Contagem visual manual good/bad, comparação com detecções aceitas e diferença de ROI fixa vs automática.
- Estado da máquina antes/depois, posição, motor, servo, falhas e necessidade de intervenção.
- `aquecimento_realizado=false` e `movimento_simulado=true` nos testes com firmware atual.

## Critérios de avanço

- **Agora:** análise da documentação acumulada e preparação das proteções mecânicas e térmicas.
- **Movimentos reais:** apenas depois de instalar/validar referência e fins de curso, parada física e revisar firmware.
- **Secagem real:** somente depois de sensores térmicos apropriados e intertravamentos independentes.
- **Produção:** requer também qualificação de exatidão CNN e critério de umidade validado.

Nunca tratar um `[OK]` do detector ROI como medida de acurácia nem o estado `PENDENTE_VALIDACAO_UMIDADE` como aprovação final do grão.
