# GrãoVision — Testes instrumentais sem aquecimento (NTC / Omron MC-245)

**Esta revisão adiciona uma aba ao GrãoVision existente.** Conserva os três temas, cadastros, relatórios, CNN e protocolo Arduino. **Não libera a lâmpada H4 nem altera o firmware.**

## Objetivo e alcance

Registrar temperatura do NTC (A2) e peso do HX711 do Arduino em 0, 30, 60, 90/91 e 120/121 segundos, permitindo que o operador anote manualmente o que aparece no Omron MC-245 nos mesmos marcos. O sistema registra o horário da coleta e da digitação manual, além do atraso entre ambas. Exibe também as médias P1, P2, PF, P3, P4, P5 e permite exportar CSV/JSON.

### Atenção ao instrumento

O **Omron MC-245 é um termômetro clínico**. Sua faixa é de **32,0 a 42,0 °C**; abaixo de 32 °C apresenta **L**. A temperatura natural registrada nos seus ensaios recentes (24–27 °C) fica **fora dessa faixa**. Para o ensaio sem calor, o instrumento pode ser usado como demonstração de indicação fora da faixa, mas **não é referência quantitativa para validar o NTC**. Registre `L`, sem transformar em 24, 25, 26 ou qualquer outro valor. O visor inicial pode mostrar 37,0 °C, um valor padrão de inicialização, não leitura medida.

Para uma comparação quantitativa a 20–30 °C, utilize no futuro um termômetro de ambiente calibrado ou com especificação metrológica para essa faixa. O MC-245 deve ser mantido dentro das condições ambientais especificadas e sem contato com H4.

### Como iniciar pelo GrãoVision

1. Faça backup da versão anterior e extraia este ZIP em pasta nova. Não substitua seu `.env`; copie o seu anterior para `analise_graos` se necessário.
2. **Mantenha a fonte 12 V da lâmpada H4 fisicamente desenergizada.** Verifique ausência de aquecimento, referência física inferior, percurso livre, fixação e parada física acessível.
3. Abra o GrãoVision com `INICIAR_GRAOVISION.bat`. Em **Configurações**, com verificação presencial, permita movimentos reais e selecione a COM correta.
4. Abra **Testes instrumentais** e clique em **INICIAR ENSAIO SEM H4 (CICLO EXISTENTE)**. Confirme os avisos.
5. O firmware primeiro centraliza o servo; **a tara requer prato vazio**. Aguarde o prompt de tara, confirme, e **somente depois** coloque e fixe a amostra/instrumento no prato quando solicitado. Não toque ou pressione o termômetro durante a pesagem ou movimentos.
6. **Limitação do firmware V10.2.7:** não existe comando serial independente de `SUBIR PARA TESTE`. O botão utiliza a sequência completa **P1 → CNN → P2 → PF → P3 → P4 → P5**. **A subida depende da CNN aprovar a amostra.** Se a CNN rejeitar ou o processo for inconclusivo, **não force nem falsifique** um CAM:OK. Esse caso fica registrado como ensaio instrumental incompleto.
7. Quando chegar cada `MEDICAO` aos 0, 30, 60/61, 90/91 e 120/121 s, o PC emite um aviso sonoro e cria a linha. Escolha o ponto e registre `L`, `H` ou número entre 32,0 e 42,0 °C. Anote observações. O atraso de digitação aparecerá na tabela.
8. O CSV e o JSON são salvos automaticamente ao final do ciclo na pasta `%USERPROFILE%\.graovision_tcc\ensaios_instrumentais`. É possível atualizar as leituras manuais depois do término e salvar novamente.

### Importante para interpretação

* Sem calor, este ensaio mede repetibilidade e temperatura local em condição ambiental. **Não mede secagem nem teor de umidade.**
* A indicação de massa inclui tudo o que foi colocado no prato após a tara. O termômetro não deve ser pressionado ou tocado durante as leituras: contato adicional perturba a célula.
* Sensor NTC e termômetro na bandeja estão **em locais diferentes**: mesmo com instrumentos adequados, a comparação não é uma calibração ponto a ponto.
* Mensagens de `POS_ATUADOR:0` são **posição comandada**, não homing comprovado.
* O programa **não implementa subida isolada**. Isso exige futura alteração controlada no firmware com testes de segurança específicos. Não enviamos comandos de motor desconhecidos.
* O pacote conserva a pasta histórica `arduino/V10_2_5_Controle_Termico_Real` da edição anterior. **Não regrave esses sketches antigos no Arduino**; preserve o firmware V10.2.7 que já está funcionando até uma revisão deliberada.

### Arquivos técnicos novos

* `analise_graos/interface/ensaios_instrumentais.py`: aquisição, validade MC-245, média, marca temporal e exportação.
* `analise_graos/interface/testes_instrumentais.py`: aba gráfica integrada.
* `analise_graos/tests/test_ensaios_instrumentais.py`: testes automatizados do novo registro.

### O que foi testado

88 testes Python completos, inicialização Tk em tela virtual, telemetria simulada, exportação JSON/CSV e mudança dos três temas sem perder os valores. **Não foi realizado teste físico com seu Arduino/MC-245; não trate como homologação elétrica ou mecânica.**
