#!/usr/bin/env python

# build-info.py
# (c) Vasiliy Turchenko 2018

import sys
import os
import re
#import strings
from pathlib2 import Path
from datetime import date
from datetime import datetime

out_file_name = "buildinfo.h"
build_file_name = "buildinfo.num"

if len(sys.argv) < 2:
    print("Usage " + sys.argv[0] + " <path>")
    sys.exit(1)

out_path = sys.argv[1]
skip_chars = len(out_path) + 1

out_file_name = Path(os.path.join(out_path, out_file_name))
build_file_name = Path(os.path.join(out_path, build_file_name))

current_buld = -1

if (not(build_file_name.is_file())):
	print("Warning! The file " + str(build_file_name) + " does not exist. The current build number will be set to 0!\n")
	build_file = open(str(build_file_name), "w+")
	build_file.write(str(current_buld))
	build_file.close()
else:
	build_file = open(str(build_file_name), "r")
	current_buld_s = build_file.read()
	current_buld = int(current_buld_s,10)
	build_file.close()

#open and reset output file
out_file = open(str(out_file_name), "w+")
current_buld = current_buld + 1

out_file.write("/* This file is auto-generated! Do not edit! */\n")
out_file.write("static const char * buildNum = \"Build number: " + str(current_buld) + "\\n\";\n")

today = date.today()
d = today.strftime("%d %B, %Y")
now = datetime.now()
current_time = now.strftime("%H:%M:%S")
out_file.write("static const char * buildDateTime = \"Build time and date: " + current_time + "  " + d + "\\n\";\n")
out_file.close()

build_file = open(str(build_file_name), "w+")
build_file.write(str(current_buld))
build_file.close()
