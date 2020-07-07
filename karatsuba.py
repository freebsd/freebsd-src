#! /usr/bin/python3 -B
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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
import subprocess
import time

def usage():
	print("usage: {} [num_iterations test_num exe]".format(script))
	print("\n    num_iterations is the number of times to run each karatsuba number; default is 4")
	print("\n    test_num is the last Karatsuba number to run through tests")
	sys.exit(1)

def run(cmd, env=None):
	return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

script = sys.argv[0]
testdir = os.path.dirname(script)

if testdir == "":
	testdir = os.getcwd()

print("\nWARNING: This script is for distro and package maintainers.")
print("It is for finding the optimal Karatsuba number.")
print("Though it only needs to be run once per release/platform,")
print("it takes forever to run.")
print("You have been warned.\n")
print("Note: If you send an interrupt, it will report the current best number.\n")

if __name__ != "__main__":
	usage()

mx = 520
mx2 = mx // 2
mn = 16

num = "9" * mx

args_idx = 4

if len(sys.argv) >= 2:
	num_iterations = int(sys.argv[1])
else:
	num_iterations = 4

if len(sys.argv) >= 3:
	test_num = int(sys.argv[2])
else:
	test_num = 0

if len(sys.argv) >= args_idx:
	exe = sys.argv[3]
else:
	exe = testdir + "/bin/bc"

exedir = os.path.dirname(exe)

indata = "for (i = 0; i < 100; ++i) {} * {}\n"
indata += "1.23456789^100000\n1.23456789^100000\nhalt"
indata = indata.format(num, num).encode()

times = []
nums = []
runs = []
nruns = num_iterations + 1

for i in range(0, nruns):
	runs.append(0)

tests = [ "multiply", "modulus", "power", "sqrt" ]
scripts = [ "multiply" ]

print("Testing CFLAGS=\"-flto\"...")

flags = dict(os.environ)
try:
	flags["CFLAGS"] = flags["CFLAGS"] + " " + "-flto"
except KeyError:
	flags["CFLAGS"] = "-flto"

p = run([ "./configure.sh", "-O3" ], flags)
if p.returncode != 0:
	print("configure.sh returned an error ({}); exiting...".format(p.returncode))
	sys.exit(p.returncode)

p = run([ "make" ])

if p.returncode == 0:
	config_env = flags
	print("Using CFLAGS=\"-flto\"")
else:
	config_env = os.environ
	print("Not using CFLAGS=\"-flto\"")

p = run([ "make", "clean" ])

print("Testing \"make -j4\"")

if p.returncode != 0:
	print("make returned an error ({}); exiting...".format(p.returncode))
	sys.exit(p.returncode)

p = run([ "make", "-j4" ])

if p.returncode == 0:
	makecmd = [ "make", "-j4" ]
	print("Using \"make -j4\"")
else:
	makecmd = [ "make" ]
	print("Not using \"make -j4\"")

if test_num != 0:
	mx2 = test_num

try:

	for i in range(mn, mx2 + 1):

		print("\nCompiling...\n")

		p = run([ "./configure.sh", "-O3", "-k{}".format(i) ], config_env)

		if p.returncode != 0:
			print("configure.sh returned an error ({}); exiting...".format(p.returncode))
			sys.exit(p.returncode)

		p = run(makecmd)

		if p.returncode != 0:
			print("make returned an error ({}); exiting...".format(p.returncode))
			sys.exit(p.returncode)

		if (test_num >= i):

			print("Running tests for Karatsuba Num: {}\n".format(i))

			for test in tests:

				cmd = [ "{}/tests/test.sh".format(testdir), "bc", test, "0", "0", exe ]

				p = subprocess.run(cmd + sys.argv[args_idx:], stderr=subprocess.PIPE)

				if p.returncode != 0:
					print("{} test failed:\n".format(test, p.returncode))
					print(p.stderr.decode())
					print("\nexiting...")
					sys.exit(p.returncode)

			print("")

			for script in scripts:

				cmd = [ "{}/tests/script.sh".format(testdir), "bc", script + ".bc",
				        "0", "1", "1", "0", exe ]

				p = subprocess.run(cmd + sys.argv[args_idx:], stderr=subprocess.PIPE)

				if p.returncode != 0:
					print("{} test failed:\n".format(test, p.returncode))
					print(p.stderr.decode())
					print("\nexiting...")
					sys.exit(p.returncode)

			print("")

		elif test_num == 0:

			print("Timing Karatsuba Num: {}".format(i), end='', flush=True)

			for j in range(0, nruns):

				cmd = [ exe, "{}/tests/bc/power.txt".format(testdir) ]

				start = time.perf_counter()
				p = subprocess.run(cmd, input=indata, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				end = time.perf_counter()

				if p.returncode != 0:
					print("bc returned an error; exiting...")
					sys.exit(p.returncode)

				runs[j] = end - start

			run_times = runs[1:]
			avg = sum(run_times) / len(run_times)

			times.append(avg)
			nums.append(i)
			print(", Time: {}".format(times[i - mn]))

except KeyboardInterrupt:
	nums = nums[0:i]
	times = times[0:i]

if test_num == 0:

	opt = nums[times.index(min(times))]

	print("\n\nOptimal Karatsuba Num (for this machine): {}".format(opt))
	print("Run the following:\n")
	if "-flto" in config_env["CFLAGS"]:
		print("CFLAGS=\"-flto\" ./configure.sh -O3 -k {}".format(opt))
	else:
		print("./configure.sh -O3 -k {}".format(opt))
	print("make")
