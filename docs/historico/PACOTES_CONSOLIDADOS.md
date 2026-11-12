# GraoVision - V10.1C a V10.2 e GUI/IHM - consolidados

## Proveniencia e escopo

Inventario gerado a partir de ZIPs locais; nenhuma execucao ou validacao de hardware foi feita.
Foram copiados somente arquivos-fonte e textos tecnicos selecionados, preservando seus bytes
no repositorio local. Os nomes e caminhos internos originais constam no manifesto CSV
junto com o destino exato no repositorio. Caminhos longos recebem nome curto
deterministico em `__caminhos_longos/` (sem alterar o arquivo original).

| ZIP original (relativo a pasta de arquivos) | Fontes importadas | Ignorados | `.env` ignorados | SHA-256 do ZIP |
|---|---:|---:|---:|---|
| `CONSOLIDADOS/GUI-IHM/Diagnostico_Roboflow_kernel_corn_v54.zip` | 2 | 1 | 0 | `b5fe9d264c7a39b6821e057745f6f7387ba9ad987ce752fcd07b7507360012b5` |
| `CONSOLIDADOS/GUI-IHM/GraoVision_3_Temas_Cadastros_Relatorios_MAPA_TCC.zip` | 64 | 15 | 1 | `0fcaced614eef1b7a82c39ad6998b90edc4c4dd7e3dc5e238f325c9ca25fa6e6` |
| `CONSOLIDADOS/GUI-IHM/GraoVision_3_Temas_GUI.zip` | 58 | 12 | 1 | `70023e52cea34b140477f921145a473c7552b55aeb1a9ab8202db09b3565b682` |
| `CONSOLIDADOS/GUI-IHM/GraoVision_Graficos_Detalhados_2N2907_SEM_H4.zip` | 73 | 16 | 1 | `2da528c081ca3a4315059ec68028270228921da56268fc552c26607e95a3f8dd` |
| `CONSOLIDADOS/GUI-IHM/GraoVision_Rastreabilidade_MAPA_TCC/analise_graos/Diagnostico_Integral_Roboflow_kernel_corn_v54.zip` | 3 | 2 | 0 | `d117b0e93ef5cac5591963d920442cf20eff822dbc8ec0fdccea0472d58749ad` |
| `CONSOLIDADOS/GUI-IHM/GraoVision_Testes_Instrumentais_MC245_SEM_H4.zip` | 68 | 15 | 1 | `e23dd1e711249ecb7d5d016eeacbad1a8e868e46c3f8b1a37484a470dbbba04b` |
| `CONSOLIDADOS/GUI-IHM/TCC_GraoVision_GUI_V10_2_7_R1.zip` | 53 | 4 | 1 | `05dd01b122085efc6977a0cc58698d71da8273531a5bee3bb323932d5b25184e` |
| `CONSOLIDADOS/Protocolo_Validacao_Desidratacao_TCC_2026.zip` | 2 | 0 | 0 | `3c0818acc956e1a7694c4804ca86f525ad87e5fd9da2e2ae546c75ab4748ceae` |
| `CONSOLIDADOS/TCC_R1_FIX_EEPROM.zip` | 45 | 47 | 1 | `97c50d7cc171087581d39c48c042aae3b82fc7e43ec7ee20764a6f8b80e9cf08` |
| `CONSOLIDADOS/V10_1C_Arduino_Final_Consolidado.zip` | 24 | 3 | 1 | `fdbe053b2e2d84fc7a22c935f1b56d0ed7a6ac1d252eec23294f8f5c981fe7ab` |
| `CONSOLIDADOS/V10_1C_R2_Servo_Centro_Passos_Reais.zip` | 26 | 4 | 1 | `f78b0261647fd15f1e1d73df080da401322a62edbad5f0d5caab416138f2b50b` |
| `CONSOLIDADOS/V10_1C_R3_Movimento_Suave_Tara_Estavel_Sem_Trancos.zip` | 29 | 3 | 1 | `ddba25cb0dace009cf1710763bfcfde9ce64bdffcd9c7f4e481edd1aa0314651` |
| `CONSOLIDADOS/V10_2_1_Servo_PWM_Suspenso_Pesagem_Compensacao.zip` | 38 | 4 | 1 | `76bbf31fb3035ed5ad60f35d3732b3e7cb499b58cdaf9142044b0511c3189bd2` |
| `CONSOLIDADOS/V10_2_2_Erros_Retorno_Supervisionado.zip` | 42 | 5 | 1 | `04ec6521450f387c484f5d3ba99c82e891b8d603c8422c04cd9536fcb6c4b1c6` |
| `CONSOLIDADOS/V10_2_3_Diagnostico_NTC_Camara_Sem_Aquecimento.zip` | 3 | 0 | 0 | `50f7c4abfc208938d8a1d3f3dccfef26465b88874c9c8d2392f7af51ee69d49c` |
| `CONSOLIDADOS/V10_2_4_Ensaio_Integrado_Intertravado_TCC.zip` | 33 | 3 | 1 | `172525af76d916ce9b1554c06c3e11203edb94542494b8e3a9a71704a57c737e` |
| `CONSOLIDADOS/V10_2_5_Controle_Termico_PF_Final_Supervisionado.zip` | 35 | 3 | 1 | `a7f1270c2ea725086ac9b0653563e1236d71e5b99b3eabf6e937aa6b105a8681` |
| `CONSOLIDADOS/V10_2_6_Vetores_P1_P2_PF_P3_Integrado.zip` | 38 | 3 | 1 | `bb8bf6cbc86534cde64dfef5c0861270f8545256b299d0caf358b7c96c8fa817` |
| `CONSOLIDADOS/V10_2_7_Ajuste_Acomodacao_Servo_Motor.zip` | 41 | 3 | 1 | `d8f6a742d806541bda5f192f50d709c89325ae261dba174e407cfd8c2bf92574` |
| `CONSOLIDADOS/V10_2_7_Pausa_Transicoes_2s_Final.zip` | 42 | 29 | 1 | `aa3d8dadedd5db8777ea3457f7ed48f998f727161898f94e5a46015528f22a4c` |
| `CONSOLIDADOS/V10_2_7_R1_Timeouts_EEPROM_Pausa1s.zip` | 43 | 3 | 1 | `28376772405a84836b05b5e8cd471b9691ebb117cd913f68603dc8c35a8e498f` |
| `CONSOLIDADOS/V10_2_7_Vetores_P1_P2_PF_P3_P4_P5_Integrado.zip` | 40 | 3 | 1 | `331e896883fbec4f70d435ee3480202566d653a1ffdb94778a5d4709b547db8d` |
| `CONSOLIDADOS/V10_2_Compensacao_Massa_Preparacao_Termica.zip` | 35 | 3 | 1 | `e3379a21cb1bb9216a849432ef5864d9c307e70aef8aeb93a2ba5703da974994` |
| `CONSOLIDADOS/V10_2_Compensacao_Massa_Preparacao_Termica/relatorios.zip` | 1 | 3 | 0 | `8f6d915212677c7697284c4f6526aed8333beee66f88a80c3fdc5537f2ff23df` |

## Notas de seguranca

- Arquivos `.env`, chaves privadas, tokens, bases, imagens, logs, relatorios binarios e ambientes virtuais nao sao importados.
- Pacotes com zero fontes selecionadas continuam relacionados acima e permanecem nos ZIPs originais.
- O verificador de credenciais e estatico e incompleto; a publicacao exige auditoria manual de todo o Git e dados pessoais.
- As fontes nao foram alteradas intencionalmente pelo importador; o Git pode normalizar finais de linha segundo a configuracao local.
- Os titulos dos pacotes indicam a finalidade declarada, nao comprovam resultados experimentais.
- Arquivos graficos, evidencias de ensaio e dados tabulares precisarao de triagem especifica antes de eventual publicacao.
