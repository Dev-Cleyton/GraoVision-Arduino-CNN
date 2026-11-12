# Requisitos indispensaveis antes de energizar a resistencia

- Confirmar se fonte termica e lampada halogena H2 ou outro elemento, sua tensao, corrente e potencia nominais, e dimensionamento eletrico do circuito.
- Instalar termostato de seguranca / limitador termico independente em serie com a carga, capaz de abrir fisicamente o circuito se a temperatura subir demasiadamente. Um segundo sensor conectado ao mesmo Arduino nao substitui esse corte.
- Validar atuacao segura do relé/contator em reset, boot, perda de USB, falha da fonte, NTC aberto, NTC em curto, relé travado e software bloqueado; elementos ativos em LOW podem momentaneamente acionar na partida.
- Instalar barreira contra contato com superficie aquecida, cabos resistentes a temperatura e protecoes adequadas para corrente/fonte utilizada. Verificar livre circulacao de ar e ausencia de materiais inflamaveis.
- Validar posicao segura do prato e elevador: retencao contra queda, fins de curso / homing, e parada fisica de emergencia.
- Definir temperatura de secagem e duracao por protocolo validado para o tipo de grao; a perda medida nao e por si so teor absoluto de umidade.
- Documentar ensaios do corte independente antes de permitir firmware que energize D7.

A V10.2.4 prepara o relatorio e integra os modulos JA validados, mas nao energiza D7.
