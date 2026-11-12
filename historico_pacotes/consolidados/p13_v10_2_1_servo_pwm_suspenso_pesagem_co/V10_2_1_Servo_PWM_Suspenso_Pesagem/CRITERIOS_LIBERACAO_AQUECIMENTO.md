# Projeto do modulo termico: condicoes para futura liberacao

**Nao ligue a resistencia nesta versao.** O NTC em A2 acompanha a regiao da celula de carga,
nao mede a temperatura da camara aquecida. O sistema nao possui atualmente
protecao independente e comprovada contra sobretemperatura; o modulo de rele e
ativo LOW e pode ter transitorios de boot.

## Antes de qualquer firmware com aquecimento habilitado
- Identificar tensao, potencia da resistencia, ventilacao e materiais inflamaveis.
- Instalar sensor termico independente da balanca na zona termica relevante,
  calibrado com referencia conhecida para faixa de operacao.
- Implementar hardware de corte por sobretemperatura independente do Arduino
  (limitador termico com rearme seguro e/ou fusivel termico conforme projeto).
- Contator/SSR/driver com categoria, distancia de isolacao e capacidade apropriadas,
  revisado por profissional habilitado se ligado a rede eletrica.
- Garantir estado aquecedor OFF por hardware durante boot, reset, perda de comunicacao,
  troca de modo, falha de sensor, comando STOP e queda de energia.
- Verificar protecoes contra acesso, queda de bandeja, fim de curso e parada de emergencia.
- Validar desligamento real sob falha antes de carregar amostras.
- Definir temperatura maxima, tempo de permanencia, histerese e criterio de parada
  por protocolo de secagem adequado ao tipo de grao, com metodo de referencia.
- Separar temperatura da camara de temperatura do NTC junto a celula de carga.

Quando essas verificacoes estiverem documentadas, a etapa de comando termico
pode ser projetada com fail-safe, leitura redundante e ensaios graduais em bancada.
Nenhum numero de tempo/temperatura devera ser inventado como limite operacional.
