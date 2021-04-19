#!/usr/bin/env python

import fileinput
import re

definition = ''
state = 0
params = 0

for line in fileinput.input():
        if test := re.search(r'^\tpublic\s+(.*)', line):
            definition = 'public ' + test.group(1)
            state = 1
            params = 0
        elif (state == 1) and (test := re.search(r'(\w+)\s*\(', line)):
            definition = '{0} LESSPARAMS (('.format(test.group(1))
            state = 2
        elif state == 2:
            if re.search(r'^{', line):
                if not params: definition += 'VOID_PARAM'
                print(f'{definition}));')
                state = 0
            elif test := re.search(r'^\s*([^;]*)', line):
                if (definition[-1:] != '('): definition += ', ' 
                definition += test.group(1) 
                params = 1
