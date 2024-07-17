@echo off
for %%G in ( *.yaml ) do (
	call :append_config_file_name %%G
	if %errorlevel% NEQ 0 (
		echo Error while building %%G
		goto :eof
	)
)
set "configs_list=%configs_list:~1%"
set items_cout=0
for %%i in (%configs_list%) do set /a cnt+=1
if /i "%1"=="show" (
	echo List of tests: 
	echo ----------------------------------------------------
	for %%i in (%configs_list%) do (
		echo     %%i
	)
	echo ----------------------------------------------------
	echo Total: %cnt% items
	goto :eof
)
for %%i in (%configs_list%) do (
	echo ----------------------------------------------------
	echo Compiling: %%i
	echo ----------------------------------------------------
	if /i "%1"=="wsl" (
	        wsl -e sh -lc "esphome compile %%i"
	) else (
		esphome compile %%i
	)
	if %ERRORLEVEL% NEQ 0 ( 
		echo ----------------------------------------------------
		echo Failed to build: %%i
		echo ----------------------------------------------------
		goto :eof
	)
)
goto :eof

:append_config_file_name
	set config_file=%1
	if "%config_file:~0,1%" NEQ "." (
		set "configs_list=%configs_list%,%config_file%"
	) else (
		echo Skipping %config_file%
	)
goto :eof
