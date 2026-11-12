# Testes executados na entrega V10.1 — 07/11/2026

## Comprovado (offline)

1. 7 testes `unittest` no Python, com serial mock e CNN mock: caminho aprovado CNN sem validacao umidade, reprovação visual, inconclusivo, recepcao incompleta (erro), aplicacao de limite externo validado, arquivo complementar de umidade e integridade de relatorio JSON/CSV.
2. Sintaxe de 13 modulos Python verificada com `compile()` no Python presente nesta maquina.
3. Sintaxe de `.ino` testada com `g++ -std=c++11 -fsyntax-only` e stubs basicos de Arduino/Servo/HX711, com sucesso. **Nao equivale a compilacao com placas AVR e bibliotecas originais.**
4. Auditoria de segurança por inspecao: `PIN_RELE_AQUECEDOR` so e comandado HIGH, sem codigo para ligar aquecedor. D6 driver comandado HIGH por padrao.

## Nao foi testado (pendente)

1. Compilacao real na Arduino IDE para ATmega328P com a biblioteca HX711 instalada pelo usuario.
2. Comunicacao fisica COM4, reinicio serial, cabos, energia e robustez de timeout no hardware.
3. Captura real via webcam e chamada real Roboflow (chave nao incluida).
4. Movimentos reais do servo e stepper com novo fluxo central-subir-girar e retorna-centro-descer.
5. Homing, interrupcao segura, limites de curso, falha de energia e proteção elétrica em potencia.
6. Aquececimento e medicao da temperatura real da zona de secagem, controle seguro de aquecedor.
7. Metodo de umidade absoluta da amostra, comparacao com referencias e limites empresariais.

## Referencia historica

A auditoria historica V10 (arquivo separado entregue antes) documenta numerosos testes de ruído, estabilidade, movimento, relés, temperatura, deriva V12/V13 e retorno ao zero. Nao confundir essa evidencia com uma validacao fisica desta nova integracao.
