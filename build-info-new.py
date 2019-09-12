#!/usr/bin/env python

# build-info.py
# (c) Vasiliy Turchenko 2018

import sys
import os
import re
import json
#import strings
from pathlib2 import Path
from datetime import date
from datetime import datetime

buildinfo_file_n = "buildinfo.h"

if len(sys.argv) < 2:
    print("Usage " + sys.argv[0] + " <path>")
    sys.exit(1)

out_path = sys.argv[1]
skip_chars = len(out_path) + 1

buildinfo_file_name = Path(os.path.join(out_path, buildinfo_file_n))
current_build_num = -1
new_build_string = "/* {\"BUILD\" : -1} */\n/* This file is auto-generated! Do not edit! */\n"

if (not(buildinfo_file_name.is_file())):
	print("Warning! The file " + str(buildinfo_file_n) + " does not exist. The file will be created and current build number will be set to 0!\n")
	buildinfo_file = open(str(buildinfo_file_name), "w+")
	buildinfo_file.write(str(new_build_string))
	buildinfo_file.close()
else:
	buildinfo_file = open(str(buildinfo_file_name), "r")
	read_data = buildinfo_file.read()
#	print(read_data)
	splitted = read_data.splitlines()
#	print(splitted)
        current_build_s = splitted[0]
	current_build_s	= current_build_s.replace("/*","")
       	current_build_s = current_build_s.replace("*/","")
#	print(current_build_s)
	parsed_json = json.loads(current_build_s)
	current_build_num = parsed_json['BUILD']
#	print(current_build_num)
	buildinfo_file.close()

#open and reset output file
buildinfo_file = open(str(buildinfo_file_name), "w+")
new_build_num = current_build_num + 1
json = "{\"BUILD\" : "+ str(new_build_num) +"}"

build_string = "/* " + json + " */\n"
buildinfo_file.write(build_string)
buildinfo_file.write("/* This file is auto-generated! Do not edit! */\n")
buildinfo_file.write("static const char * buildNum_s = \"Build number: " + str(new_build_num) + "\\n\";\n")
buildinfo_file.write("static int buildNum_i = " + str(new_build_num) + ";\n")

today = date.today()
d = today.strftime("%d %B, %Y")
now = datetime.now()
current_time = now.strftime("%H:%M:%S")
buildinfo_file.write("static const char * buildDateTime = \"Build time and date: " + current_time + "  " + d + "\\n\";\n")
buildinfo_file.close()
