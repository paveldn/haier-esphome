@echo off
SET base_path=%~dp0
cd %base_path%..
echo ===============================================================================================================
echo                                  Updaiting README.rst
echo ===============================================================================================================
python %base_path%script/make_doc.py README.rst
echo ===============================================================================================================
echo                                  Updaiting docs/hon_example.rst
echo ===============================================================================================================
python %base_path%script/process_examples.py %base_path%examples/hon_example.rst %base_path%hon_example.rst
echo ===============================================================================================================
echo                                  Updaiting docs/smartair2_example.rst
echo ===============================================================================================================
python %base_path%script/process_examples.py %base_path%examples/smartair2_example.rst %base_path%smartair2_example.rst
echo ===============================================================================================================
echo                                  Updaiting docs/usb_2_uart_boards.rst
echo ===============================================================================================================
python %base_path%script/process_examples.py %base_path%examples/usb_2_uart_boards.rst %base_path%usb_2_uart_boards.rst
echo ===============================================================================================================
echo                                  Updaiting docs/sniffing_serial_communication.rst
echo ===============================================================================================================
python %base_path%script/process_examples.py %base_path%examples/sniffing_serial_communication.rst %base_path%sniffing_serial_communication.rst
cd %base_path%