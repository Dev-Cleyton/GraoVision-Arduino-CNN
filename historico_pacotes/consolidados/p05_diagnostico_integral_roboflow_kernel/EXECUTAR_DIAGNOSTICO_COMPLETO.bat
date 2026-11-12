@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo =====================================================
echo  GRAOVISION - DIAGNOSTICO INTEGRAL ROBOFLOW V54
 echo  Apenas consulta, sem treino, sem imagens, sem Arduino
 echo =====================================================
where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%~dp0diagnostico_integral_roboflow_v54.py"
) else (
    python "%~dp0diagnostico_integral_roboflow_v54.py"
)
echo.
echo Encerrado. Verifique os arquivos RESULTADO na pasta.
pause
