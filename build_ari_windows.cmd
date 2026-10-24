@echo off
setlocal
cd /d "%~dp0"
where msbuild >nul 2>nul
if errorlevel 1 (
  echo MSBuild not found. Open the Visual Studio 2022 Developer Command Prompt for VS 2022.
  exit /b 1
)
msbuild Zeal.sln /m /p:Configuration=Release /p:Platform=x86 /p:LanguageStandard=stdcpp20 /p:ExportHeader="" /p:ScanDependencies=""
if errorlevel 1 exit /b 1
echo Compiled: Release\Zeal.asi
