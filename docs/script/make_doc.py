import re
import os

doc_file_path = [ 
    'esphome-docs/climate/haier.rst',
    'esphome-docs/sensor/haier.rst',
    'esphome-docs/binary_sensor/haier.rst',
    'esphome-docs/text_sensor/haier.rst',
    'esphome-docs/button/haier.rst',
]

def process_images(line, pth):
    m = re.match(r"^( *\.\. +figure:: +)(.+)$", line)
    if m:
        new_url = "./docs/" + pth + "/" + m.group(2)
        return ".. raw:: HTML\n\n  <p><a href=\"" + new_url + "?raw=true\"><img src=\"" + new_url + "?raw=true\" height=\"50%\" width=\"50%\"></a></p>\n"
        #return m.group(1) + new_url + "\n"
    return line

def process_esphome_refs(line, l_num):
    esphome_refs = [
        (":ref:`uart`", "`UART Bus <https://esphome.io/components/uart#uart>`_"),
        (":ref:`config-id`", "`ID <https://esphome.io/guides/configuration-types.html#config-id>`_"),
        (":ref:`config-time`", "`Time <https://esphome.io/guides/configuration-types.html#config-time>`_"),
        (":ref:`Automation <automation>`", "`Automation <https://esphome.io/guides/automations#automation>`_"),
        (":ref:`haier-on_alarm_start`", "`on_alarm_start Trigger`_"),
        (":ref:`haier-on_alarm_end`", "`on_alarm_end Trigger`_"),
        (":ref:`Climate <config-climate>`", "`Climate <https://esphome.io/components/climate/index.html#config-climate>`_"),
        (":ref:`lambdas <config-lambda>`", "`lambdas <https://esphome.io/guides/automations#config-lambda>`_"),
        (":ref:`Sensor <config-sensor>`", "`Sensor <https://esphome.io/components/sensor/index.html#config-sensor>`_"),
        (":ref:`Binary Sensor <config-binary_sensor>`", "`Binary Sensor <https://esphome.io/components/binary_sensor/index.html#base-binary-sensor-configuration>`_"),
        (":ref:`Text Sensor <config-text_sensor>`", "`Text Sensor <https://esphome.io/components/text_sensor/index.html#base-text-sensor-configuration>`_"),
        (":ref:`Button <config-button>`", "`Text Sensor <https://esphome.io/components/button/index.html#base-button-configuration>`_"),

    ]
    res = line
    for o, r in esphome_refs:
        res = res.replace(o, r)
    if res.find(":ref:") != -1:
        print(f"Warning: ref found, line #{l_num}!")
    return res

lines_to_remove = [
    "    :align: center\n",
    "    :width: 50.0%\n",
    "    :width: 70.0%\n"
]
output_file = open("../../main.rst", "w")
for in_f in doc_file_path:
    input_file = open(f"../{in_f}", "r")
    p = os.path.dirname(in_f)
    lines = input_file.readlines()
    is_seo = False
    l_counter = 1
    for line in lines:
        if line == "See Also\n":
            break
        if line == ".. seo::\n":
            is_seo = True
            continue
        if is_seo:
            if not line.startswith("    :"):
                is_seo = False
            continue
        if line in lines_to_remove:
            continue
        l = process_images(line, p)
        l = process_esphome_refs(l, l_counter)
        output_file.write(l)
        l_counter += 1
