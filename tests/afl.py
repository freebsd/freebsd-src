#! /usr/bin/python3 -B
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2021 Gavin D. Howard and contributors.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

import os
import sys
import shutil
import subprocess

def usage():
	print("usage: {} [--asan] dir [results_dir [exe options...]]".format(script))
	sys.exit(1)

def check_crash(exebase, out, error, file, type, test):
	if error < 0:
		print("\n{} crashed ({}) on {}:\n".format(exebase, -error, type))
		print("    {}".format(test))
		print("\nCopying to \"{}\"".format(out))
		shutil.copy2(file, out)
		print("\nexiting...")
		sys.exit(error)

def run_test(cmd, exebase, tout, indata, out, file, type, test, environ=None):
	try:
		p = subprocess.run(cmd, timeout=tout, input=indata, stdout=subprocess.PIPE,
		                   stderr=subprocess.PIPE, env=environ)
		check_crash(exebase, out, p.returncode, file, type, test)
	except subprocess.TimeoutExpired:
		print("\n    {} timed out. Continuing...\n".format(exebase))

def create_test(file, tout, environ=None):

	print("    {}".format(file))

	base = os.path.basename(file)

	if base == "README.txt":
		return

	with open(file, "rb") as f:
		lines = f.readlines()

	print("        Running whole file...")

	run_test(exe + [ file ], exebase, tout, halt.encode(), out, file, "file", file, environ)

	print("        Running file through stdin...")

	with open(file, "rb") as f:
		content = f.read()

	run_test(exe, exebase, tout, content, out, file,
	         "running {} through stdin".format(file), file, environ)


def get_children(dir, get_files):
	dirs = []
	with os.scandir(dir) as it:
		for entry in it:
			if not entry.name.startswith('.') and     \
			   ((entry.is_dir() and not get_files) or \
			    (entry.is_file() and get_files)):
				dirs.append(entry.name)
	dirs.sort()
	return dirs


def exe_name(d):
	return "bc" if d == "bc1" or d == "bc2" or d == "bc3" else "dc"

script = sys.argv[0]
testdir = os.path.dirname(script)

if __name__ != "__main__":
	usage()

timeout = 2.5

if len(sys.argv) < 2:
	usage()

idx = 1

exedir = sys.argv[idx]

asan = (exedir == "--asan")

if asan:
	idx += 1
	if len(sys.argv) < idx + 1:
		usage()
	exedir = sys.argv[idx]

print("exedir: {}".format(exedir))

if len(sys.argv) >= idx + 2:
	resultsdir = sys.argv[idx + 1]
else:
	if exedir == "bc1":
		resultsdir = testdir + "/fuzzing/bc_outputs1"
	elif exedir == "bc2":
		resultsdir = testdir + "/fuzzing/bc_outputs2"
	elif exedir == "bc3":
		resultsdir = testdir + "/fuzzing/bc_outputs3"
	else:
		resultsdir = testdir + "/fuzzing/dc_outputs"

print("resultsdir: {}".format(resultsdir))

if len(sys.argv) >= idx + 3:
	exe = sys.argv[idx + 2]
else:
	exe = testdir + "/../bin/" + exe_name(exedir)

exebase = os.path.basename(exe)

if exebase == "bc":
	halt = "halt\n"
	options = "-lq"
else:
	halt = "q\n"
	options = "-x"

if len(sys.argv) >= idx + 4:
	exe = [ exe, sys.argv[idx + 3:], options ]
else:
	exe = [ exe, options ]
for i in range(4, len(sys.argv)):
	exe.append(sys.argv[i])

out = testdir + "/../.test.txt"

print(os.path.realpath(os.getcwd()))

dirs = get_children(resultsdir, False)

if asan:
	env = os.environ.copy()
	env['ASAN_OPTIONS'] = 'abort_on_error=1:allocator_may_return_null=1'

for d in dirs:

	d = resultsdir + "/" + d

	print(d)

	files = get_children(d + "/crashes/", True)

	for file in files:
		file = d + "/crashes/" + file
		create_test(file, timeout)

	if not asan:
		continue

	files = get_children(d + "/queue/", True)

	for file in files:
		file = d + "/queue/" + file
		create_test(file, timeout * 2, env)

print("Done")

