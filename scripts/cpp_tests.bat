@echo off
setlocal enabledelayedexpansion

set BUILD_TYPE=RelWithDebInfo
set BUILD_DIR=build-cpp-tests/

if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
cmake -S . -B %BUILD_DIR% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if errorlevel 1 goto :error

cmake --build %BUILD_DIR% ^
      --config %BUILD_TYPE% ^
      --target mlc_backtrace-static ^
      -j %NUMBER_OF_PROCESSORS% ^
      -- -verbosity:detailed
if errorlevel 1 goto :error

rmdir /s /q %BUILD_DIR%
goto :eof

:error
echo Script failed with error #%errorlevel%.
exit /b %errorlevel%
:eof

endlocal
