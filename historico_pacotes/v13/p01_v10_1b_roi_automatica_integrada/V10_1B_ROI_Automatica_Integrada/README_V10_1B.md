# V10.1B — CNN COM ROI AUTOMATICA INTEGRADA

## Qual foi a alteracao

Esta versao foi derivada da **V10.1 integrada validada**. O firmware Arduino
`arduino/codigo_automacaoV10_1_Integrada/codigo_automacaoV10_1_Integrada.ino`
permanece **sem nenhuma alteracao**. A V10.1B modifica o Python para:

- Detectar automaticamente a borda circular da bandeja na imagem ORIGINAL (detector V10.1A V2).
- Registrar o centro, raio interno/externo, cobertura e suporte das bordas.
- Utilizar o circulo interno detectado para aceitar/rejeitar caixas da CNN.
- Enviar ao Roboflow a fotografia ORIGINAL completa (sem recorte/mascara).
- Registrar uma comparacao **diagnostica** com a ROI fixa da V10.1,
  usando as mesmas predicoes, **sem influenciar a decisao**.
- Interromper por erro se a bandeja nao for localizada; **nunca faz fallback**
  silencioso para ROI fixa e **nunca interpreta falha de ROI como CAM:RUIM**.
- Incluir `roi_modo`, `roi_status`, `roi_registros` no relatorio JSON. O CSV
  das deteccoes ganha a coluna `dentro_roi_fixa_referencia`.
- Preservar comportamento CNN (`bad > 25%` reprova), pesagens P1/P2, serie
  sem aquecimento, relatorio e limites da V10.1.

## SEGURANCA / LIMITACOES

- **Firmware nao sofreu alteracoes.** Todos os relés devem permanecer sem
  cargas de potencia em COM/NO/NC. Aquecimento real BLOQUEADO. Movimentos
  simulados por padrao no .ino. Nao ativar movimentos reais sem homing,
  fim de curso e revisao mecanica.
- ROI automatica e experimental. Sucesso de localizacao **nao e precisao de
  classificacao da CNN**; verificar as imagens e, no futuro, validar manualmente.
- Detecta bandejas aproximadamente circulares. Inclinacoes, sombras e reflexos
  podem causar bloqueio, ou circulo impreciso mesmo com retorno OK.
- O circulo interior corresponde a 81% do raio do aro externo e pode
  descartar graos perifericos: confira se toda a amostra está incluida.
- Umidade dos graos continua **nao determinada** sem metodo metrologico valido.
- A chave Roboflow fica apenas no `.env` local. **NAO inclua `.env` em ZIP.**

## INSTALACAO (PowerShell/Windows 10 — Python 3.13)

1. Extraia este ZIP numa **nova pasta**, separada do projeto V10.1 anterior.
2. No Arduino mantenha carregado o `.ino` V10.1 que ja compilou: **nao precisa
   regravar** apenas para testar ROI.
3. Copie **somente** seu arquivo `analise_graos/.env` privado da V10.1
   anterior para `analise_graos/.env` da nova pasta, ou crie a partir de
   `.env.example`; coloque `ROBOFLOW_API_KEY=SUA_CHAVE_REAL` localmente.
4. Caso ainda nao tenha dependencias:

```powershell
& "C:/Program Files/Python313/python.exe" -m pip install -r ".\analise_graos\requirements.txt"
```

5. `ROI_AUTOMATICA=1` e o padrao da V10.1B, mesmo que sua chave `.env`
   antiga nao tenha essa variavel. Se quiser escrever explicitamente, acrescente
   `ROI_AUTOMATICA=1` ao `.env`.
6. Teste o detector sem placa/camera/internet:

```powershell
& "C:/Program Files/Python313/python.exe" ".\analise_graos\validar_roi_offline.py" --imagem ".\imagens\20261107_184754_3c0389\imagem_1.jpg" --saida ".\teste_roi_v101b"
```

   O novo pacote nao inclui suas fotografias anteriores: se necessario,
   informe o caminho completo da imagem no projeto V10.1 anterior.
7. Testes Python offline (sem Roboflow e sem Arduino):

```powershell
cd ".\analise_graos"
& "C:/Program Files/Python313/python.exe" -m unittest discover -s tests -v
```

8. Para executar a V10.1B real: feche o Monitor Serial da IDE, confirme
   fisicamente ausencia de cargas de potencia, volte a pasta RAIZ
   `V10_1B_ROI_Automatica_Integrada` e execute:

```powershell
& "C:/Program Files/Python313/python.exe" ".\analise_graos\main.py"
```

   O Arduino enviara `READY:V10.1` **mesmo com Python V10.1B**: isso e
   esperado, pois o firmware serial foi mantido exatamente igual.

## SAIDAS GERADAS POR AMOSTRA

Em `imagens/ID/`:
- `imagem_1.jpg`: fotografia original usada pelo Roboflow;
- `imagem_1_roi_visualizada.jpg`: borda externa laranja, ROI interna ciano;
- `imagem_1_roi_automatica.json`: parametros, comparacao com ROI fixa e contagens;
- `imagem_1_detectada.jpg`: caixas da CNN + ROI atual;
- `imagem_1_deteccoes.csv`: classe, confianca, coordenadas e indicador de ambas ROIs.

Em `relatorios/`:
- `ID_relatorio.json`: inclui `roi_status`, `roi_modo`, `roi_registros`,
  decisao da CNN, peso P1/P2/PF, historico e status final;
- `ID_pesagens.csv` (se aprovado visualmente): pesos/temperatura.

## FALHA DE ROI

Se a bandeja nao for localizada, sai `REVISAR` em
`imagem_1_roi_automatica.json`, interrompe antes do Roboflow e envia
`STOP` ao Arduino, encerrando com `ERRO_PROCESSO`. **Nao** gera
`CAM:RUIM` e **nao** reprova grãos com base em fotografia invalida.
Pode ser necessario resetar a placa para iniciar novo ciclo apos STOP.

## COMPARACAO COM O HISTORICO V10.1

Na mesma fotografia e na mesma inferencia, o relatório tem
`comparacao_roi_fixa`: contagem total e `bad` pela ROI antiga.
Para fazer um teste intencional com ROI antiga (fora da rotina de producao),
pode-se usar `ROI_AUTOMATICA=0` no `.env`. **Nunca e usado automaticamente
quando ROI_AUTOMATICA=1 falha**.

## VERIFICACOES

- Sintaxe de todos os arquivos Python usando `python -m compileall`.
- Testes de fluxo Arduino serial simulado + novos testes de ROI e seguranca.
- A confirmacao de inferencia real Roboflow, sensor, motor e aquecedor
  depende da execucao no hardware do usuario.
