import sys
import re
import os

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

with open(input_file, "r") as f:
    fpath = os.path.dirname(os.path.abspath(input_file))
    ofile = open(output_file, "w")
    lines = f.readlines()
    ofile.writelines(document_header)
    for line in lines:
      m = re.match(r"^.. example_yaml:: ([^\n]+)", line)
      if m:
        print("Processing example: " + m.group(1))
        ofile.write(".. code-block:: yaml\n\n")
        example = open(os.path.join(fpath, m.group(1)), "r")
        for l in example.readlines():
          ofile.write("    " + l)
        ofile.write("\n")
      else:
        ofile.write(line)