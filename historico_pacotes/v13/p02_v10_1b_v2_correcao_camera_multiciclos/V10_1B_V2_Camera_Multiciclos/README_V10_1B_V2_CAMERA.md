# V10.1B V2 — correção para reabertura da webcam entre ciclos

## Ocorrência observada

O **primeiro ciclo real V10.1B** concluiu com ROI, CNN, P1/P2/PF e `FIM:1`.
No **segundo ciclo**, depois de `P1` e `CMD:ACTIVATE_CAMERA`,
`cv2.VideoCapture(0, cv2.CAP_DSHOW)` falhou na abertura, exibindo aviso DirectShow.
O log mostra que a câmera foi liberada após o ciclo anterior; **não há prova
que tenha permanecido aberta**. O erro é compatível com reabertura temporariamente
indisponível, conflito de outro aplicativo, falha de driver/USB ou mudança do índice.

## Mudanças limitadas ao Python

- `analise_graos/hardware/camera_handler.py`: tenta DirectShow até 3 vezes,
  depois Media Foundation até 2; libera cada captura malsucedida e valida
  que há frames legíveis; faz warm-up e libera o dispositivo se o warm-up falhar.
  Informa o backend e a resolução real anunciada pela câmera. O programa **não
  altera resolução da CNN nem o índice 0** e mantém o filtro de luminosidade.
- `analise_graos/core/sample_service.py`: remove a referência à câmera após liberar.
- Incluído teste automatizado `test_camera_multiciclos_v101b_v2.py`.
- **Nenhum outro arquivo de aplicação foi alterado.** Arduino/ROI/CNN/pesagem/relatórios,
  chave `.env` e protocolo serial permanecem os mesmos.

## Como testar no Windows

1. Extrair o ZIP numa **nova pasta**, preservando V10.1B anterior.
2. Copiar seu `.env` privado que já funciona para `analise_graos/.env`.
3. Com a máquina **sem cargas de potência** e movimentos simulados, fechar
   aplicativos que possam acessar a webcam (Câmera do Windows, Teams, navegador etc.).
4. Se já estiver dentro da pasta `analise_graos`, executar:

```powershell
& "C:/Program Files/Python313/python.exe" ".\main.py"
```

5. Fazer um primeiro ciclo e responder **S** a `Iniciar outro ciclo? (S/N)`.
   Confirmar se o segundo abre a câmera. O log deverá exibir mensagens
   `[CAMERA] Abrindo indice 0: DirectShow, tentativa 1/3...` etc.
6. Se continuar indisponível, conferir conexão USB, disponibilidade da webcam,
   permissões de câmera do Windows e possível mudança de índice. Não alterar
   o índice às cegas; verificar dispositivo antes.

### Testes offline (independentes de webcam, Arduino e chave)

Execute dentro de `analise_graos`:

```powershell
& "C:/Program Files/Python313/python.exe" -m unittest discover -s tests -v
```

**Observação:** tentativas de reabertura melhoram resiliência, mas não garantem
que um driver fisicamente travado se recupere. Falha persistente gera
`ERRO_PROCESSO` e `STOP`, preservando o motivo no relatório.

**Segurança:** não ligar o aquecedor nem as cargas de potência. O NTC não é
sensor de umidade dos grãos, e os movimentos físicos continuam simulados.
