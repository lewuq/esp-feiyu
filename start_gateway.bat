@echo off
setlocal
cd /d "%~dp0"
set "DAFEIYU_PYTHON="
if exist "D:\Espressif\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe" set "DAFEIYU_PYTHON=D:\Espressif\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"
if defined DAFEIYU_PYTHON goto run
py -3.11 -c "import audioop" >nul 2>&1
if not errorlevel 1 goto pylauncher
python -c "import audioop" >nul 2>&1
if not errorlevel 1 goto python
 echo Python 3.11 is required. Install it or use an ESP-IDF Python 3.11 environment.
pause
exit /b 1
:pylauncher
py -3.11 gateway\server.py --open
goto finish
:python
python gateway\server.py --open
goto finish
:run
"%DAFEIYU_PYTHON%" gateway\server.py --open
:finish
if errorlevel 1 pause
endlocal
