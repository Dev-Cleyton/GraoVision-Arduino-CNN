# V10.2 - compensacao de massa (aquecimento permanece bloqueado)

## Objetivo
Integra a compensacao de offset entre P1 e P2 ao sistema V10.1C-R3, preservando
os arquivos Arduino existentes, a EEPROM, os movimentos suavizados, a CNN/ROI,
e as mensagens seriais. **Nao habilita o rele do aquecedor e nao aumenta o risco termico.**

## Como instalar
1. Extraia numa pasta NOVA; preserve a R3 ja funcional.
2. Use o mesmo sketch R3 que ja passou pelo teste mecanico. O .ino do pacote e uma
   copia **inalterada** do firmware R3 e **nao precisa ser regravado** no Arduino.
3. Copie apenas o `.env` existente da R3 para `analise_graos/.env`, sem divulgar a chave.
4. Execute `python analise_graos/main.py` a partir da pasta raiz, ou `python main.py`
   a partir da subpasta `analise_graos`.
5. Mantenha as protecoes mecanicas e o aquecedor totalmente desconectado.

## Dados obtidos
- P1_g: leitura bruta centro.
- P2_g: leitura bruta na posicao de secagem ANTES de calor.
- PF_g: leitura bruta final na mesma posicao de P2.
- offset = P2 - P1.
- PF_corrigido = PF - offset.
- Perda aparente = P1 - PF_corrigido = P2 - PF.
- Percentual ref P1 = 100*(P2-PF)/P1.
- Percentual ref P2 = 100*(P2-PF)/P2 (campo antigo compatibilidade).

Um resultado negativo significa aumento aparente de leitura, NAO agua adicionada.
A compensacao so remove o offset ADITIVO entre posicoes, e NAO elimina a deriva
que ocorre com o tempo, calor, correntes de ar, cabos ou tensoes mecanicas.
Esses numeros NAO SAO teor de umidade dos graos.

Relatorio novo `*_balanco_massa.csv` e campos extras no `*_relatorio.json`.
Os relatórios antigos nao sao sobrescritos, e os campos antigos continuam.

## Simular uma perda de massa sem aquecedor
Na pasta `analise_graos`, executar:

`python simular_secagem_offline.py --p1 10.8414 --p2 10.8994 --pf 10.4994`

Este simulador calcula apenas matematica. Nao manda comandos ao Arduino.

## Validacao
`cd analise_graos`
`python -m unittest discover -s tests -v`

## Para liberar aquecimento em OUTRA versao, precisa existir e ser validado
1. Sensor de temperatura real da CAMARA, colocado e calibrado no local adequado.
2. Limitador independente de sobretemperatura, que corte fisicamente a energia.
3. Elemento de potencia dimensionado por tensao/corrente, fusivel e protecao eletrica.
4. Intertravamentos e parada de emergencia de hardware; boot/desconexao/STOP devem
   deixar aquecedor desenergizado. Relés ativos em LOW podem pulsar no boot.
5. Procedimento de verificacao do posicionamento e de seguranca no equipamento.
6. Calibracao metrologica da perda aparente contra referencia gravimetrica validada.

A posicao P1/P2 so corrige um offset constante. **Aquecimento permanece bloqueado**.
