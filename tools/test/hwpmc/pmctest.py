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

import sys
import subprocess
from subprocess import PIPE
import argparse
import tempfile
from pathlib import Path
import os

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
    parser.add_argument('--count', action='store_true', help='Exercise the program being studied using counting mode pmcs.')
    parser.add_argument('--sample', action='store_true', help='Exercise the program being studied using sampling mode pmcs.')

    args = parser.parse_args()

    counters = gather_counters()

    if len(counters) <= 0:
        print("no counters found")
        sys.exit()

    if args.count == True and args.sample == True:
        print("Choose one of --count OR --sample.")
        sys.exit()

    program = Path(args.program).name

    if args.count == True:
        tmpdir = tempfile.mkdtemp(prefix=program + "-", suffix="-counting-pmc")
        print("Exercising program ", args.program, " storing results data in ", tmpdir)

    if args.sample == True:
        tmpdir = tempfile.mkdtemp(prefix=program + "-", suffix="-sampling-pmc")
        print("Exercising program ", args.program, " storing results data in ", tmpdir)

    for counter in counters:
        if counter in notcounter:
            continue
        if args.count == True:
            with open(tmpdir + "/" + program + "-" + counter + ".txt", 'w') as file:
                p = subprocess.Popen(["pmcstat",
                                      "-p", counter, args.program],
                                     text=True, stderr=file, stdout=file)
                result = p.wait()
                print(result)
        elif args.sample == True:
            pmcout = tmpdir + "/" + program + "-" + counter + ".pmc"
            p = subprocess.Popen(["pmcstat",
                                  "-O", pmcout,
                                  "-P", counter, args.program],
                                 text=True, stderr=PIPE)
            result = p.wait()
            resdir = tmpdir + "/" + program + "-" + counter + ".results"
            os.makedirs(resdir)
            p = subprocess.Popen(["pmcstat",
                                  "-R", pmcout,
                                  "-g"],
                                 cwd=resdir,
                                 text=True, stderr=PIPE)
            result = p.wait()
            gmondir = resdir + "/" + counter
            if Path(gmondir).is_dir():
                with open(gmondir + "/" + "gprof.out", "w") as file:
                    p = subprocess.Popen(["gprof",
                                          args.program,
                                          program + ".gmon"],
                                         cwd=gmondir,
                                         text=True,
                                         stdout=file,
                                         stderr=subprocess.STDOUT)
                    result = p.wait()
            else:
                print ("Failed to get gmon data for ", counter)
            print(result)

        else:
            p = subprocess.Popen(["pmcstat", "-p", counter, args.program],
                             text=True, stderr=PIPE)
            result = p.wait()
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
