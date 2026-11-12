@echo off
setlocal
cd /d "%~dp0analise_graos"
if exist "C:\Program Files\Python313\python.exe" (
  "C:\Program Files\Python313\python.exe" iniciar_interface.py
) else (
  py -3 iniciar_interface.py
)
if errorlevel 1 (
  echo.
  echo Erro ao iniciar. Execute INSTALAR_DEPENDENCIAS.bat ou veja o LEIA_ME_GUI.md.
  pause
)
endlocal
