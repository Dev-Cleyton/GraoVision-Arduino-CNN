# V10.1C-R2 — Integração do servo, motor e telemetria (sem aquecimento)

Este pacote reaproveita a V10.1C e adiciona: centralização do servo ANTES da tara, contagem serial a cada 500 pulsos comandados (sem alterar os 14.000 passos), tratamento de recuperação da EEPROM no Python e registro de movimentos nos relatórios.

## Conteúdo

- `arduino/V10_1C_R2_Simulacao_Segura/V10_1C_R2_Simulacao_Segura.ino`: simulação, **sem movimento físico**.
- `arduino/V10_1C_R2_Movimento_Real_Supervisionado/V10_1C_R2_Movimento_Real_Supervisionado.ino`: habilita comandos físicos **apenas após armamento manual por ciclo**. **Aquecedor sempre desligado**.
- `analise_graos/`: Python integrado com webcam, ROI automática, CNN, relatórios e nova telemetria.
- `.env.example`: exemplo de configuração; não contém credenciais.
- `PROTOCOLO_E_SEGURANCA.md`: mensagens seriais e limites do ensaio.

## ATENÇÃO: NÃO é firmware autônomo de produção

**Aquecedor fisicamente desconectado**; NTC A2 mede perto da célula de carga e não monitora a câmara. Firmware não tem homing, fins de curso ou proteção térmica independente. A EEPROM armazena PULSOS COMANDADOS, não deslocamento físico. Servo não possui retorno de ângulo.

**Antes de executar o sketch com movimentos reais**, são necessárias proteções físicas adequadas para impedir esmagamento/colisão e queda do elevador: parada de emergência elétrica independente do software, limite mecânico seguro, comprovação de retenção da carga vertical mesmo se o relé do driver desligar, e inspeção do curso. Se estas condições não estiverem atendidas, use exclusivamente a simulação. `STOP` serial não equivale a uma parada de emergência física.

## Instalação

1. Faça backup da V10.1C anterior. Extraia em pasta nova.
2. No Arduino IDE, escolha o sketch correspondente ao tipo de ensaio. Placa **Arduino Uno**. Bibliotecas `HX711`, `Servo`, `EEPROM`. Faça **Verificar/Compilar** antes de gravar. Não compile os dois sketches de uma vez.
3. Copie `analise_graos/.env.example` para `analise_graos/.env`, coloque a chave Roboflow do projeto anterior **sem divulgá-la** e mantenha `ROI_AUTOMATICA=1`.
4. Se carregar **simulação**, use `MOVIMENTOS_REAIS=0`. Se carregar **real supervisionado**, use `MOVIMENTOS_REAIS=1`; esse segundo modo requer as proteções físicas descritas acima.
5. Feche Monitor Serial do Arduino IDE; o Python precisa usar a COM. Abra PowerShell dentro de `analise_graos` e rode:

```powershell
& "C:/Program Files/Python313/python.exe" ".\main.py"
```

6. Na PRIMEIRA utilização do sketch real, se surgir `RECUPERACAO:PENDENTE`, o programa só prossegue após você posicionar FISICAMENTE a bandeja na base X inferior, servo no centro e prato vazio, e digitar exatamente `CONFIRMO-X`. `RECUPERAR:X` ajusta apenas a referência lógica, **não movimenta** o sistema.
7. Em cada ciclo físico, inspecione o mecanismo e confirme `ARMAR`; então o programa envia `CENTRALIZAR` e recebe `SERVO:CENTRO:OK` **antes de `TARA`**.
8. O operador coloca os grãos, confirma `START` e o ciclo continua conforme resultado CNN. Não coloque mãos próximas de partes móveis.

## O que esperar

```text
[PC -> UNO] ARMAR:REAL
[UNO -> PC] ARMADO:REAL
[PC -> UNO] CENTRALIZAR
[UNO -> PC] MOVIMENTO:REAL:SERVO
[UNO -> PC] SERVO:CENTRO:OK
[PC -> UNO] TARA
[UNO -> PC] TARA:OK
...
[UNO -> PC] PASSOS:SUBIDA:0:14000:0
[UNO -> PC] PASSOS:SUBIDA:500:14000:500
...
[UNO -> PC] PASSOS:SUBIDA:14000:14000:14000
...
[UNO -> PC] PASSOS:DESCIDA:500:14000:13500
...
[UNO -> PC] PASSOS:DESCIDA:14000:14000:0
[UNO -> PC] FIM:1
```

*Os números acima são exemplos da estrutura das mensagens esperadas; não são um log de ensaio físico já realizado.*

Os logs `PASSOS` também ficam no arquivo `relatorios/<id>_movimentos.csv` e no campo `movimentos_motor` do JSON. É possível avaliar progresso e configurar futuramente o curso, mas **a alteração automática de limites de passos não foi liberada** para evitar colisões.

## Verificações realizadas aqui

- 25 testes Python offline passaram (`python -m unittest discover -s tests -p "test_*.py"`).
- Sintaxe C++ verificada com compilador genérico e bibliotecas simuladas, **não substitui compilação no Arduino IDE**.
- **Não foi realizado teste com o motor ou servo do seu equipamento**, portanto os novos movimentos reais ainda exigem validação presencial.

## Observações sobre recuperação de energia

Após interrupção, a EEPROM guarda checkpoint periódico de 100 passos e o ângulo comandado do servo. O erro de posição pode ser maior que 100 passos se houver perda de passos, escorregamento ou movimento livre após desligamento; não há confirmação física por sensor. O firmware **não retoma automaticamente**: exige referência segura e confirmação explícita.
