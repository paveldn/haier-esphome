@echo off
if [%1] == [] goto err
wsl -e sh -lc "esphome compile %1 2>&1 | tee buildlog.txt"
goto:eof
:err
echo Please use with esphome configuration as parameter
echo Example: build.cmd haier.yaml