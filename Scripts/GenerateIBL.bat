@echo off
REM GenerateIBL.bat - IBLBaker를 이용한 IBL 자동 생성 스크립트
REM 사용법: GenerateIBL.bat [HDR파일명]
REM 예시: GenerateIBL.bat studio

setlocal enabledelayedexpansion

REM 설정
set IBLBAKER_PATH=Tools\IBLBaker\IBLBaker.exe
set HDR_DIR=Assets\HDR
set IBL_DIR=Assets\IBL

REM 인자 확인
if "%1"=="" (
    echo Usage: GenerateIBL.bat [HDR_filename_without_extension]
    echo Example: GenerateIBL.bat studio
    echo.
    echo Available HDR files:
    dir /b "%HDR_DIR%\*.hdr" 2>nul
    pause
    exit /b 1
)

set HDR_NAME=%1
set INPUT_HDR=%HDR_DIR%\%HDR_NAME%.hdr
set OUTPUT_DIR=%IBL_DIR%\%HDR_NAME%

REM 입력 파일 존재 확인
if not exist "%INPUT_HDR%" (
    echo Error: HDR file not found: %INPUT_HDR%
    echo.
    echo Available HDR files:
    dir /b "%HDR_DIR%\*.hdr" 2>nul
    pause
    exit /b 1
)

REM IBLBaker 실행파일 존재 확인
if not exist "%IBLBAKER_PATH%" (
    echo Error: IBLBaker not found: %IBLBAKER_PATH%
    echo Please download and extract IBLBaker to Tools\IBLBaker\
    echo GitHub: https://github.com/derydoca/IBLBaker
    pause
    exit /b 1
)

echo ========================================
echo Generating IBL for: %HDR_NAME%
echo Input: %INPUT_HDR%
echo Output: %OUTPUT_DIR%
echo ========================================
echo.

REM 출력 디렉토리 생성
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM IBLBaker 실행
echo Running IBLBaker...
"%IBLBAKER_PATH%" ^
    -inputFile "%INPUT_HDR%" ^
    -outputDir "%OUTPUT_DIR%\" ^
    -irradianceSize 64 ^
    -prefilterSize 256 ^
    -environmentSize 512 ^
    -numSamples 1024 ^
    -outputFormat HDR

REM 결과 확인
if %ERRORLEVEL% neq 0 (
    echo.
    echo Error: IBLBaker failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo IBL Generation Complete!
echo ========================================
echo.
echo Generated files:
dir /b "%OUTPUT_DIR%\*.hdr" 2>nul

echo.
echo Files ready for LunarDX12:
echo - Environment Map: %OUTPUT_DIR%\environment_*.hdr
echo - Irradiance Map: %OUTPUT_DIR%\irradiance_*.hdr  
echo - Prefilter Maps: %OUTPUT_DIR%\prefilter_*.hdr
echo.

REM 파일 크기 정보
echo File sizes:
for %%f in ("%OUTPUT_DIR%\*.hdr") do (
    echo   %%~nxf: %%~zf bytes
)

echo.
echo To use in LunarDX12:
echo   m_iblSystem-^>LoadFromIBLBaker("%HDR_NAME%", ...);
echo.

pause
