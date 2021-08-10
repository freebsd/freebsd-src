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


# Print the usage and exit with an error.
def usage():
	print("usage: {} [--asan] dir [results_dir [exe options...]]".format(script))
	print("       The valid values for dir are: 'bc1', 'bc2', 'bc3', and 'dc'.")
	sys.exit(1)


# Check for a crash.
# @param exebase  The calculator that crashed.
# @param out      The file to copy the crash file to.
# @param error    The error code (negative).
# @param file     The crash file.
# @param type     The type of run that caused the crash. This is just a string
#                 that would make sense to the user.
# @param test     The contents of the crash file, or which line caused the crash
#                 for a run through stdin.
def check_crash(exebase, out, error, file, type, test):
	if error < 0:
		print("\n{} crashed ({}) on {}:\n".format(exebase, -error, type))
		print("    {}".format(test))
		print("\nCopying to \"{}\"".format(out))
		shutil.copy2(file, out)
		print("\nexiting...")
		sys.exit(error)


# Runs a test. This function is used to ensure that if a test times out, it is
# discarded. Otherwise, some tests result in incredibly long runtimes. We need
# to ignore those.
#
# @param cmd      The command to run.
# @param exebase  The calculator to test.
# @param tout     The timeout to use.
# @param indata   The data to push through stdin for the test.
# @param out      The file to copy the test file to if it causes a crash.
# @param file     The test file.
# @param type     The type of test. This is just a string that would make sense
#                 to the user.
# @param test     The test. It could be an entire file, or just one line.
# @param environ  The environment to run the command under.
def run_test(cmd, exebase, tout, indata, out, file, type, test, environ=None):
	try:
		p = subprocess.run(cmd, timeout=tout, input=indata, stdout=subprocess.PIPE,
		                   stderr=subprocess.PIPE, env=environ)
		check_crash(exebase, out, p.returncode, file, type, test)
	except subprocess.TimeoutExpired:
		print("\n    {} timed out. Continuing...\n".format(exebase))


# Creates and runs a test. This basically just takes a file, runs it through the
# appropriate calculator as a whole file, then runs it through the calculator
# using stdin.
# @param file     The file to test.
# @param tout     The timeout to use.
# @param environ  The environment to run under.
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


# Get the children of a directory.
# @param dir        The directory to get the children of.
# @param get_files  True if files should be gotten, false if directories should
#                   be gotten.
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


# Returns the correct executable name for the directory under test.
# @param d  The directory under test.
def exe_name(d):
	return "bc" if d == "bc1" or d == "bc2" or d == "bc3" else "dc"


# Housekeeping.
script = sys.argv[0]
scriptdir = os.path.dirname(script)

# Must run this script alone.
if __name__ != "__main__":
	usage()

timeout = 2.5

if len(sys.argv) < 2:
	usage()

idx = 1

exedir = sys.argv[idx]

asan = (exedir == "--asan")

# We could possibly run under ASan. See later for what that means.
if asan:
	idx += 1
	if len(sys.argv) < idx + 1:
		usage()
	exedir = sys.argv[idx]

print("exedir: {}".format(exedir))

# Grab the correct directory of AFL++ results.
if len(sys.argv) >= idx + 2:
	resultsdir = sys.argv[idx + 1]
else:
	if exedir == "bc1":
		resultsdir = scriptdir + "/../tests/fuzzing/bc_outputs1"
	elif exedir == "bc2":
		resultsdir = scriptdir + "/../tests/fuzzing/bc_outputs2"
	elif exedir == "bc3":
		resultsdir = scriptdir + "/../tests/fuzzing/bc_outputs3"
	elif exedir == "dc":
		resultsdir = scriptdir + "/../tests/fuzzing/dc_outputs"
	else:
		raise ValueError("exedir must be either bc1, bc2, bc3, or dc");

print("resultsdir: {}".format(resultsdir))

# More command-line processing.
if len(sys.argv) >= idx + 3:
	exe = sys.argv[idx + 2]
else:
	exe = scriptdir + "/../bin/" + exe_name(exedir)

exebase = os.path.basename(exe)


# Use the correct options.
if exebase == "bc":
	halt = "halt\n"
	options = "-lq"
	seed = ["-e", "seed = 1280937142.20981723890730892738902938071028973408912703984712093", "-f-" ]
else:
	halt = "q\n"
	options = "-x"
	seed = ["-e", "1280937142.20981723890730892738902938071028973408912703984712093j", "-f-" ]

# More command-line processing.
if len(sys.argv) >= idx + 4:
	exe = [ exe, sys.argv[idx + 3:], options ] + seed
else:
	exe = [ exe, options ] + seed
for i in range(4, len(sys.argv)):
	exe.append(sys.argv[i])

out = scriptdir + "/../.test.txt"

print(os.path.realpath(os.getcwd()))

dirs = get_children(resultsdir, False)

# Set the correct ASAN_OPTIONS.
if asan:
	env = os.environ.copy()
	env['ASAN_OPTIONS'] = 'abort_on_error=1:allocator_may_return_null=1'

for d in dirs:

	d = resultsdir + "/" + d

	print(d)

	# Check the crash files.
	files = get_children(d + "/crashes/", True)

	for file in files:
		file = d + "/crashes/" + file
		create_test(file, timeout)

	# If we are running under ASan, we want to check all files. Otherwise, skip.
	if not asan:
		continue

	# Check all of the test cases found by AFL++.
	files = get_children(d + "/queue/", True)

	for file in files:
		file = d + "/queue/" + file
		create_test(file, timeout * 2, env)

print("Done")
