# V10.2.6 - Vetores de massa e P3 (diagnostico)

Esta versao e uma modificacao conservadora da V10.2.5 anexada pelo usuario.

## Alteracoes
- P1, P2, PF: preservam media original de 12 leituras e passam a enviar cada valor aceito com `VETOR:ETAPA:INDICE:MILLIS:POSICAO_COMANDADA:MASSA_G`.
- P3: medida adicional apos descida e estabilizacao, na posicao central, sem nova tara.
- P3 nao participa da classificacao CNN nem da umidade. Falha de P3 gera `P3:ERRO_DIAGNOSTICO` e o ciclo segue ao desligamento/finalizacao.
- Python salva vetores em CSV e estatisticas amostrais (media, desvio padrao, amplitude, min/max), inclusive erro P3-P1.
- As `MEDICAO` a cada 30 s continuam preservadas, usando o metodo anterior de pesagem.
- Sem alteracoes em ROI, CNN, paradas, supervisao, EEPROM ou rampas.

## Seguranca e validacao
- `AQUECEDOR_PERMITIDO_APOS_INSPECAO = false` em ambos os firmwares. D7 NAO esta validado contra pulsos durante inicializacao. Aquecedor fisicamente desconectado.
- A posicao do motor continua COMANDADA, nao confirmada por sensor; nao existe homing.
- A simulacao ou compilacao offline nao valida seguranca fisica nem precisao metrologica.
- Antes de qualquer ensaio real: backup da versao validada, inspecao de percursos, integridade da EEPROM, cabo do HX711, suporte da bandeja e parada de emergencia, com supervisao presencial.
- A V10.2.6 nao determina umidade de graos: permanece PENDENTE_VALIDACAO_UMIDADE.

## Executar
1. Copiar `analise_graos/.env.example` para `.env`, preencher somente as variaveis necessarias. Por seguranca testar primeiro com `MOVIMENTOS_REAIS=0`, `AQUECIMENTO_REAL=0`.
2. Instalar requisitos (`pip install -r requirements.txt`) e executar `python main.py` dentro de `analise_graos`.
3. Compilar o sketch correspondente ao modo escolhido e testar inicialmente sem atuadores energizados; o arquivo Real exige autorizacao presencial por ciclo.
4. Executar `python -m unittest discover -s tests -v`.

## Interpretacao
A media das leituras reduz parte da variabilidade aleatoria, mas nao elimina deriva, histerese, efeito de cabos ou erros de calibracao. Como a celula acompanha a bandeja, conferir a tensao mecanica dos cabos em todo o percurso. Comparar P3 com P1 apenas em ensaios sem mudanca intencional de massa.

## Limitacao
Verificacao automatica Python realizada; compilacao do Arduino e teste fisico necessarios no ambiente do usuario. O firmware esta sujeito a possiveis ajustes apos teste de bancada.
