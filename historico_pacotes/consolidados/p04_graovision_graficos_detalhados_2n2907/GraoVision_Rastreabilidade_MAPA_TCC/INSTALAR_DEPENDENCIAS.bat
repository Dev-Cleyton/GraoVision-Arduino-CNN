@echo off
setlocal
cd /d "%~dp0analise_graos"
if exist "C:\Program Files\Python313\python.exe" (
  "C:\Program Files\Python313\python.exe" -m pip install -r requirements.txt
) else (
  py -3 -m pip install -r requirements.txt
)
echo.
echo Concluido. Se ocorreu erro de internet ou pip, verifique a conexao.
pause
endlocal
