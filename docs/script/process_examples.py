import sys
import re
import os
import codecs

document_header = [
    ".. This file is automatically generated by ./docs/script/process_examples.py Python script.\n",
    "   Please, don't change. In case you need to make corrections or changes change\n",
    "   source documentation in ./doc folder or script.\n\n"
]

if len(sys.argv) < 3:
    print("Usage: python process_examples.py <input_file> <output_file>")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]

with codecs.open(input_file, "r", "utf-8") as f:
    fpath = os.path.dirname(os.path.abspath(input_file))
    ofile = codecs.open(output_file, "w", "utf-8")
    lines = f.readlines()
    if "no_header" not in sys.argv:
      ofile.writelines(document_header)
    for line in lines:
      m = re.match(r"^.. example_yaml:: ([^\n\r]+)", line)
      if m:
        print("Processing example: " + m.group(1))
        ofile.write(".. code-block:: yaml\n\n")
        example = codecs.open(os.path.join(fpath, m.group(1)), "r", "utf-8")
        for l in example.readlines():
          ofile.write("    " + l)
        ofile.write("\n")
      else:
        ofile.write(line)