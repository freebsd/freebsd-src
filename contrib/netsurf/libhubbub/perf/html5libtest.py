#!/usr/bin/python

import sys
import html5lib

if len(sys.argv) != 2:
	print "Usage: %s <file>" % sys.argv[0]
	sys.exit(1)

f = open(sys.argv[1])
parser = html5lib.HTMLParser()
document = parser.parse(f)
