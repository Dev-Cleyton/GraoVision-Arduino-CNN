# TCC Graos – V10.1C-R3 – Movimento suave e tara segura

## Alteracoes em relacao a R2

- **Servo:** movimento em passos de 1 grau a cada **24 ms**, sem gravacao bloqueante de EEPROM durante o giro. Aguarda **2,5 s** apos centralizacao antes da tara, ou **1,2 s** nas demais rotacoes.
- **Tara:** Python pede **ENTER depois de o operador verificar que o servo e a bandeja estao estaveis**. Firmware mede 20 leituras brutas, exige amplitude <= **0,20 g** em 3 tentativas (configuracao experimental); se nao houver estabilidade, devolve `TARA:ERRO` e NAO tara.
- **Motor:** 14.000 passos (sem mudar calibracao). Gera pulsos com rampa de aceleracao/desaceleracao de 1.200 passos em cada ponta: periodo de ~1.800 us para ~750 us. Logs continuam `PASSOS:` a cada 500 pulsos comandados.
- **EEPROM:** o firmware grava `P_EXECUTANDO` antes de mover e salva a nova posicao comandada quando o movimento termina. **Nao grava checkpoints intermediarios enquanto a base se move.** Essa escolha reduz pausas, mas em queda de energia NO MEIO DO MOVIMENTO perde a posicao exata intermediaria. Reinicio exige referenciar mecanicamente a base em X, CONFIRMO-X e RECUPERAR:X. **Nao existe retomada automatica nem sensor de posicao real.**
- **P2:** espera agora 15 s na posicao de secagem, antes da medicao.
- **Aquecedor sempre bloqueado.** NTC A2 monitora temperatura local, nao a camara.

## Como instalar

1. Copie `.env` da R2 para `analise_graos/.env` da R3. Nao compartilhe a API key.
2. Escolha apenas UM sketch da pasta `arduino/` e grave no Uno pelo Arduino IDE: `V10_1C_R3_Simulacao_Segura` para simulado, ou `V10_1C_R3_Movimento_Real_Supervisionado` SOMENTE com protecoes fisicas, limites de movimento e parada de emergencia eletrica verificados. **Nao ligue o aquecedor.**
3. Feche o Monitor Serial, abra PowerShell em `analise_graos` e execute `& "C:/Program Files/Python313/python.exe" ".\main.py"`.
4. No `.env`, `MOVIMENTOS_REAIS=1` somente se o sketch real estiver gravado e as protecoes mecanicas e de alimentacao estiverem atendidas; caso contrario use `MOVIMENTOS_REAIS=0` e o sketch simulado.
5. No ciclo real, confirme a origem X apenas quando a bandeja estiver FISICAMENTE em X inferior, vazia e com o servo no centro, e confirme apos a acomodacao do servo para iniciar a tara.

## Como validar

Apos iniciar, deve aparecer `V10.1C-R3`, e depois da autorizacao `SERVO:ACOMODADO:PRONTO_PARA_TARA` e `SERVO:CENTRO:OK`. Apos ENTER, `TARA:VERIFICANDO_ESTABILIDADE` e `TARA:OK`. Na subida `PASSOS:SUBIDA:500:14000:500` etc. Em caso de perda de energia no meio de qualquer movimento, **nao confie na posicao EEPROM como posicionamento fisico**.

## Observacoes e seguranca

- A sincronizacao de pulso e temporizada por software, nao por encoder. Os 14.000 passos sao pulsos comandados, nao deslocamento confirmado.
- Na R2 foi observada variacao de peso com aquecedor desligado: **P2 15,2006 g -> PF 15,3463 g**, um aumento de **0,1457 g**. Isso nao e perda de agua. O controle de secagem e de umidade nao esta validado.
- A rampa e os tempos de servo sao configuracoes **propostas**, nao validadas em movimento no seu equipamento. Se o mecanismo mostrar perda de passo, ruidos, trancos, colisoes ou instabilidade, interrompa o ensaio via parada eletrica de emergencia.
- A alimentacao do servo MG996R deve ser independente e dimensionada, com GND comum ao Arduino; nao use a saida 5 V do Uno para alimentar o motor do servo.
- O software nao possui controle seguro de aquecimento nem homing/fins de curso. Nao e firmware homologado para producao autonoma.
