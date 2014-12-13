#!/usr/bin/python2

import os
from subprocess import Popen, PIPE
import re

process = Popen(["g++", "--version"], stdout=PIPE)
(output, err) = process.communicate()
exit_code = process.wait()
foo = re.search(re.compile(r'\d.\d.\d'), output)

version = [int(x) for  x in foo.group(0).split('.')]
#print version

if version[0] >= 4 and version[1] > 5: print "Oh No"
