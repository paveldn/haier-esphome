@echo off
for %%G in ( *.yaml ) do (
	call :append_config_file_name %%G
	if %errorlevel% NEQ 0 (
		echo Error while building %%G
		goto :eof
	)
)
if /i "%1"=="show" (
	echo List of tests %configs_list%
	goto :eof
)
if /i "%1"=="wsl" (
	wsl --cd "%cd%" /bin/sh -c "~/.local/bin/esphome compile %configs_list%"
) else (
	esphome compile %configs_list%
)
goto :eof

:append_config_file_name
	set config_file=%1
	if "%config_file:~0,1%" NEQ "." (
		set "configs_list=%configs_list% %config_file%"
	) else (
		echo Skipping %config_file%
	)
goto :eof
