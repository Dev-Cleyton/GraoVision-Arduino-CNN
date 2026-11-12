# Resultado preliminar para a monografia — caracterizacao termica MANUAL

Em 10/11/2026, usuario informou ter acionado externamente a lampada H4 e
cronometrado 2 min 19,23 s (139,23 s). O Arduino V10.2.4 manteve seu relé D7
bloqueado e registrou os dados abaixo. Os marcos do cronometro e do Arduino
não foram sincronizados digitalmente.

| Tempo reportado pelo Arduino (s) | Massa indicada (g) | NTC na camara (C) |
|--:|--:|--:|
|0|11,1377|25,93|
|30|11,1388|34,27|
|60|11,1817|44,14|
|91|11,2413|54,03|

P1=11,0199 g; P2=11,1377 g; PF=11,2413 g. Diferença aparente
P2-PF = -0,1036 g. Desvio de massa positivo apesar do calor e
acima da prova de perda de umidade. A V10.2.4 emite PF usando a ultima
pesagem periodicada e pode nao medir ao final de 120 s. A V10.2.5
corrige esta limitacao em software, ainda sem teste fisico.

A amostra apresentou 58 deteccoes dentro ROI, 0 bad, decisao CNN de
aprovacao. Contagens nao correspondem a contagem manual verificada.

Este ensaio demonstra resposta termica preliminar do ponto NTC, nao prova
a temperatura de seguranca da camara, a temperatura do bulbo nem teor de
umidade. O relé D7 permaneceu bloqueado pelo programa; luz foi ligada
manualmente em circuito externo.
