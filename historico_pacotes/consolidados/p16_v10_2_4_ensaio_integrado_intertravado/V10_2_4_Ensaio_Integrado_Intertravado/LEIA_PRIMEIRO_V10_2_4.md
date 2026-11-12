# V10.2.4 - Ensaio integrado documentado (aquecimento bloqueado)

## Objetivo
Preservar todos os movimentos da V10.2.2 e registrar P1, CNN, ROI, P2, temperatura NTC instalada na camara em A2, PF, offset e resultados para o TCC.

**Aquecedor:** D7 (ativo LOW) continua HIGH/OFF. Nenhum comando pode ligar a resistencia nesta versao. Nao ligar a carga so porque o monitor serial exibe temperatura plausivel.

## Instalacao
1. Extraia numa pasta nova e copie apenas o seu `.env` anterior para `analise_graos/.env` (nao compartilhe a chave API).
2. Na Arduino IDE, carregue `arduino/V10_2_4_Integrado_Supervisionado_Real/V10_2_4_Integrado_Supervisionado_Real.ino` (ou a versao de simulacao).
3. Confirme `V10.2.4:INTEGRADO_MASSA_NTC_CAMARA_SEM_AQUECIMENTO` e `NTC:LOCALIZACAO:CAMARA:A2` em 9600 baud.
4. Feche o Monitor Serial, deixe o aquecedor FISICAMENTE desconectado, configure `MOVIMENTOS_REAIS=1` apenas se as condicoes mecanicas e parada de emergencia estiverem atendidas.
5. Execute, a partir de `analise_graos`, `python main.py`, respeite a checagem de X e o armamento manual.
6. Confira `*_relatorio.json`, `*_pesagens.csv`, `*_balanco_massa.csv`, `*_movimentos.csv`, `*_resultado_tcc.md`.

## Para um teste de SECAGEM energizada
Faltam dados/validacoes: potencia/tensao reais do aquecedor e adequacao do acionamento, dispositivo de corte de sobretemperatura FISICAMENTE em serie e independente do Arduino, comportamento do circuito durante boot/reset e falha do sensor, retenção mecânica da plataforma, temperatura alvo/limite de uma metodologia para o grao.
Um NTC sozinho e uma saida de rele comandada apenas por software nao formam sistema de protecao independente. O corte deve interromper a energia da carga mesmo se D7 ficar ligado permanentemente. Sem confirmacao e verificacao dessas condicoes, nao liberar o aquecimento.

## Interpretacao
Offset=P2-P1; PF_corrigido=PF-Offset; variacao=P2-PF. Em ensaio sem calor, variacao nao mede umidade nem valida desidratacao. Em ensaio com calor, ainda sera necessario avaliar deriva do sistema e comparar com metodo de referencia.

## Estado do firmware
Movimentos reais supervisionados; servo suave e PWM suspenso na posicao de secagem (somente se houver apoio mecanico); EEPROM grava fora dos movimentos; retorno apos falha SOMENTE quando autorizado e logicamente permitido. Nao ha homing.
