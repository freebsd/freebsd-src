#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2012, Neville-Neil Consulting
# Copyright (c) 2026, George V. Neville-Neil
# All rights reserved.
#

# Description: A program to run a simple program against every available
# pmc counter present in a system.
#
# To use:
#
# pmctest.py -p ls > /dev/null
#
# This should result in ls being run with every available counter
# and the system should neither lock up nor panic.
#
# The default is not to wait after each counter is tested.  Since the
# prompt would go to stdout you won't see it, just press return
# to continue or Ctrl-D to stop.

import subprocess
from subprocess import PIPE
import argparse
import tempfile

def gather_counters():
    """Run program and return output as array of lines."""
    result = subprocess.run("pmccontrol -L", shell=True, capture_output=True, text=True)
    tabbed = result.stdout.strip().split('\n')
    return [line.replace('\t', '') for line in tabbed]

# A list of strings that are not really counters, just
# name tags that are output by pmccontrol -L
notcounter = ["IAF", "IAP", "TSC", "UNC", "UCF", "UCP", "SOFT" ]

def main():

    parser = argparse.ArgumentParser(description='Exercise a program under hwpmc')
    parser.add_argument('--program', type=str, required=True, help='target program')
    parser.add_argument('--wait', action='store_true', help='Wait after each counter.')
    parser.add_argument('--exercise', action='store_true', help='Exercise the program being studied using sampling counters.')

    args = parser.parse_args()

    counters = gather_counters()

    if len(counters) <= 0:
        print("no counters found")
        sys.exit()

    if args.exercise == True:
        tmpdir = tempfile.mkdtemp()
        print("Exercising program ", args.program, " storing results data in ", tmpdir)

    for counter in counters:
        if counter in notcounter:
            continue
        if args.exercise == True:
            p = subprocess.Popen(["pmcstat",
                                  "-O", tmpdir + "/" + args.program + "-" + counter + ".pmc",
                                  "-g",
                                  "-P", counter, args.program],
                                 text=True, stderr=PIPE)
            result = p.communicate()[1]
            print(result)
        else:
            p = subprocess.Popen(["pmcstat", "-p", counter, args.program],
                             text=True, stderr=PIPE)
            result = p.communicate()[1]
            print(result)
            if (args.wait == True):
                try:
                    value = input("Waitin for you to press ENTER")
                except EOFError:
                    sys.exit()
                    
# The canonical way to make a python module into a script.
# Remove if unnecessary.
 
if __name__ == "__main__":
    main()
