@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ===============================================
echo GrãoVision - Consulta segura de metadados
 echo Projeto kernel-corn - Versao 54
 echo ===============================================
if exist "C:\Program Files\Python313\python.exe" (
  "C:\Program Files\Python313\python.exe" "diagnosticar_roboflow_v54.py"
) else (
  py -3 "diagnosticar_roboflow_v54.py"
)
echo.
echo Verifique o resultado acima. Pressione uma tecla para fechar.
pause >nul
