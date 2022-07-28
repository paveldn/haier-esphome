@echo off
if [%1] == [] goto err
powershell -command "esphome compile %1 2>&1 | tee -filepath buildlog.txt"
goto:eof
:err
echo Please use with esphome configuration as parameter
echo Example: build.cmd haier.yaml