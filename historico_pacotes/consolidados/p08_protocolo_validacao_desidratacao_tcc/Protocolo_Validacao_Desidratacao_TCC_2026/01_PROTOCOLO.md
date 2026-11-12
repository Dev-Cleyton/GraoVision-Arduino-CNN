# Protocolo de validação do módulo de desidratação — TCC / 10-11 novembro de 2026

**Situação do protótipo:** movimentos, ROI e CNN testados nas versões R3; a V10.2.2 introduziu recuperação supervisionada, **ainda sem validação física nessa versão**. O firmware V10.2.2 mantém `RELE_AQUECEDOR = D7` sempre desligado (`HIGH`, relé ativo em LOW). O NTC em A2 mede a região próxima à célula de carga, **não** a temperatura da câmara. Esta ficha **não autoriza** energizar a resistência.

## 1. Etapa A — avaliação da prontidão, sem aquecimento

- [ ] Confirmar a posição física inferior X com bandeja centralizada e percurso desimpedido; não usar `RECUPERAR:X` para simular uma posição que o mecanismo ainda não alcançou.
- [ ] Registrar fonte e potência/tensão nominal do elemento de aquecimento (sem energizá-lo).
- [ ] Identificar o sensor de temperatura **instalado na câmara** e a faixa de operação; registrar a calibração/verificação contra instrumento de referência.
- [ ] Confirmar proteção térmica **independente do Arduino**, com corte direto de energia do elemento e rearme adequado; registrar comprovação funcional **sem energizar a resistência** (ensaio funcional por profissional qualificado).
- [ ] Confirmar adequação de cabos, fusível/disjuntor, relé/contator, isolamento, aterramento quando aplicável, proteção contra contato e materiais inflamáveis, avaliados por profissional habilitado.
- [ ] Confirmar parada de emergência física e prevenção mecânica de queda/esmagamento do elevador; servo deve permanecer mecanicamente seguro sem PWM.
- [ ] Demonstrar que reset, perda de sinal do sensor, desconexão serial e STOP mantêm o aquecedor desligado; testar lógica com carga fictícia de baixa energia isolada, não com elemento de aquecimento.
- [ ] Somente com todos os itens anteriores verificados por responsável competente, definir limites térmicos específicos do grão e método laboratorial de referência. Não inferir limites a partir do NTC local da balança.

**Decisão de prontidão:** se qualquer item estiver ausente ou não verificado, fazer ensaio sem calor e registrar `VALIDACAO_TERMICA_PENDENTE`.

## 2. Etapa B — ensaio de base já disponível (sem aquecimento)

1. Colocar prato vazio, autorizar centralização e aguardar acomodação. Realizar tara; então colocar amostra.
2. Registrar P1 (massa na posição central); obter decisão CNN na câmera.
3. Se aprovada, elevar ao destino usando os 14.000 pulsos **comandados**; aguardar acomodação e PWM do servo suspenso se a bandeja for mecanicamente estável.
4. Registrar P2 no início e as leituras com seus tempos/temperatura local por 120 segundos; registrar PF e o resultado de término, inclusive falhas.
5. Calcular `offset_posicao_g = P2 - P1`. Se PF existir, calcular `variacao_massa_g = P2 - PF`; `PF_na_referencia_de_P1 = PF - offset_posicao_g`; `variacao_pct_rel_P2 = 100 * (P2-PF)/P2`.
6. Tratar variação negativa como **aumento aparente**; não denominar os valores de teor de umidade sem método validado.

## 3. Etapa C — estudo futuro de secagem real, após liberação técnica documentada

- Com a instrumentação térmica e as proteções efetivamente aprovadas, definir temperatura/tempo com base no tipo de grão e método de referência, utilizando um protocolo revisado por orientador/responsável.
- Registrar P2 **antes da energização**, histórico de temperatura da câmara, duração, estados de falha/intertravamento, PF após término e estabilização.
- Comparar resultados com pesagem independente ou método gravimétrico de referência; repetir ensaios para estimar dispersão e incerteza.
- Nunca assumir que todo P2−PF corresponde exclusivamente à água removida, ou que o offset P2−P1 corrige deriva térmica.

## 4. Evidências a salvar para o TCC

Arquivo `.ino` e versão do firmware; log completo do PowerShell; `*_relatorio.json`, `*_pesagens.csv`, `*_balanco_massa.csv`, `*_movimentos.csv`; fotografia de montagem; imagem original da câmera, visualização de ROI e detecções; instrumento e localização do termômetro da câmara; estado das proteções, incluindo aprovações e falhas. **Não divulgar o `.env` com chave de API.**

## 5. Critérios de conclusão científica

- **Mecânica e comunicação:** demonstradas por ensaios documentados, mas sem sensores físicos de homing.
- **CNN/ROI:** demonstrada a execução e classificação automatizada; precisão/recall só após comparação com rótulos manuais.
- **Massa:** leituras registradas; efeito de vibração/deriva deve ser discutido, sem superestimar exatidão.
- **Desidratação:** **não validada** enquanto aquecedor desligado e inexistente sensor/proteção térmica qualificados.
