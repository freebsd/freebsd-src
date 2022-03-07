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

import os, errno
import random
import sys
import subprocess

# I want line length to *not* affect differences between the two, so I set it
# as high as possible.
env = {
	"BC_LINE_LENGTH": "65535",
	"DC_LINE_LENGTH": "65535"
}


# Generate a random integer between 0 and 2^limit.
# @param limit  The power of two for the upper limit.
def gen(limit=4):
	return random.randint(0, 2 ** (8 * limit))


# Returns a random boolean for whether a number should be negative or not.
def negative():
	return random.randint(0, 1) == 1


# Returns a random boolean for whether a number should be 0 or not. I decided to
# have it be 0 every 2^4 times since sometimes it is used to make a number less
# than 1.
def zero():
	return random.randint(0, 2 ** (4) - 1) == 0


# Generate a real portion of a number.
def gen_real():

	# Figure out if we should have a real portion. If so generate it.
	if negative():
		n = str(gen(25))
		length = gen(7 / 8)
		if len(n) < length:
			n = ("0" * (length - len(n))) + n
	else:
		n = "0"

	return n


# Generates a number (as a string) based on the parameters.
# @param op     The operation under test.
# @param neg    Whether the number can be negative.
# @param real   Whether the number can be a non-integer.
# @param z      Whether the number can be zero.
# @param limit  The power of 2 upper limit for the number.
def num(op, neg, real, z, limit=4):

	# Handle zero first.
	if z:
		z = zero()
	else:
		z = False

	if z:
		# Generate a real portion maybe
		if real:
			n = gen_real()
			if n != "0":
				return "0." + n
		return "0"

	# Figure out if we should be negative.
	if neg:
		neg = negative()

	# Generate the integer portion.
	g = gen(limit)

	# Figure out if we should have a real number. negative() is used to give a
	# 50/50 chance of getting a negative number.
	if real:
		n = gen_real()
	else:
		n = "0"

	# Generate the string.
	g = str(g)
	if n != "0":
		g = g + "." + n

	# Make sure to use the right negative sign.
	if neg and g != "0":
		if op != modexp:
			g = "-" + g
		else:
			g = "_" + g

	return g


# Add a failed test to the list.
# @param test  The test that failed.
# @param op    The operation for the test.
def add(test, op):
	tests.append(test)
	gen_ops.append(op)


# Compare the output between the two.
# @param exe       The executable under test.
# @param options   The command-line options.
# @param p         The object returned from subprocess.run() for the calculator
#                  under test.
# @param test      The test.
# @param halt      The halt string for the calculator under test.
# @param expected  The expected result.
# @param op        The operation under test.
# @param do_add    If true, add a failing test to the list, otherwise, don't.
def compare(exe, options, p, test, halt, expected, op, do_add=True):

	# Check for error from the calculator under test.
	if p.returncode != 0:

		print("    {} returned an error ({})".format(exe, p.returncode))

		if do_add:
			print("    adding to checklist...")
			add(test, op)

		return

	actual = p.stdout.decode()

	# Check for a difference in output.
	if actual != expected:

		if op >= exponent:

			# This is here because GNU bc, like mine can be flaky on the
			# functions in the math library. This is basically testing if adding
			# 10 to the scale works to make them match. If so, the difference is
			# only because of that.
			indata = "scale += 10; {}; {}".format(test, halt)
			args = [ exe, options ]
			p2 = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
			expected = p2.stdout[:-10].decode()

			if actual == expected:
				print("    failed because of bug in other {}".format(exe))
				print("    continuing...")
				return

		# Do the correct output for the situation.
		if do_add:
			print("   failed; adding to checklist...")
			add(test, op)
		else:
			print("   failed {}".format(test))
			print("    expected:")
			print("        {}".format(expected))
			print("    actual:")
			print("        {}".format(actual))


# Generates a test for op. I made sure that there was no clashing between
# calculators. Each calculator is responsible for certain ops.
# @param op  The operation to test.
def gen_test(op):

	# First, figure out how big the scale should be.
	scale = num(op, False, False, True, 5 / 8)

	# Do the right thing for each op. Generate the test based on the format
	# string and the constraints of each op. For example, some ops can't accept
	# 0 in some arguments, and some must have integers in some arguments.
	if op < div:
		s = fmts[op].format(scale, num(op, True, True, True), num(op, True, True, True))
	elif op == div or op == mod:
		s = fmts[op].format(scale, num(op, True, True, True), num(op, True, True, False))
	elif op == power:
		s = fmts[op].format(scale, num(op, True, True, True, 7 / 8), num(op, True, False, True, 6 / 8))
	elif op == modexp:
		s = fmts[op].format(scale, num(op, True, False, True), num(op, True, False, True),
		                    num(op, True, False, False))
	elif op == sqrt:
		s = "1"
		while s == "1":
			s = num(op, False, True, True, 1)
		s = fmts[op].format(scale, s)
	else:

		if op == exponent:
			first = num(op, True, True, True, 6 / 8)
		elif op == bessel:
			first = num(op, False, True, True, 6 / 8)
		else:
			first = num(op, True, True, True)

		if op != bessel:
			s = fmts[op].format(scale, first)
		else:
			s = fmts[op].format(scale, first, 6 / 8)

	return s


# Runs a test with number t.
# @param t  The number of the test.
def run_test(t):

	# Randomly select the operation.
	op = random.randrange(bessel + 1)

	# Select the right calculator.
	if op != modexp:
		exe = "bc"
		halt = "halt"
		options = "-lq"
	else:
		exe = "dc"
		halt = "q"
		options = ""

	# Generate the test.
	test = gen_test(op)

	# These don't work very well for some reason.
	if "c(0)" in test or "scale = 4; j(4" in test:
		return

	# Make sure the calculator will halt.
	bcexe = exedir + "/" + exe
	indata = test + "\n" + halt

	print("Test {}: {}".format(t, test))

	# Only bc has options.
	if exe == "bc":
		args = [ exe, options ]
	else:
		args = [ exe ]

	# Run the GNU bc.
	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

	output1 = p.stdout.decode()

	# Error checking for GNU.
	if p.returncode != 0 or output1 == "":
		print("    other {} returned an error ({}); continuing...".format(exe, p.returncode))
		return

	if output1 == "\n":
		print("   other {} has a bug; continuing...".format(exe))
		return

	# Don't know why GNU has this problem...
	if output1 == "-0\n":
		output1 = "0\n"
	elif output1 == "-0":
		output1 = "0"

	args = [ bcexe, options ]

	# Run this bc/dc and compare.
	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
	compare(exe, options, p, test, halt, output1, op)


# This script must be run by itself.
if __name__ != "__main__":
	sys.exit(1)

script = sys.argv[0]
testdir = os.path.dirname(script)

exedir = testdir + "/../bin"

# The following are tables used to generate numbers.

# The operations to test.
ops = [ '+', '-', '*', '/', '%', '^', '|' ]

# The functions that can be tested.
funcs = [ "sqrt", "e", "l", "a", "s", "c", "j" ]

# The files (corresponding to the operations with the functions appended) to add
# tests to if they fail.
files = [ "add", "subtract", "multiply", "divide", "modulus", "power", "modexp",
          "sqrt", "exponent", "log", "arctangent", "sine", "cosine", "bessel" ]

# The format strings corresponding to each operation and then each function.
fmts = [ "scale = {}; {} + {}", "scale = {}; {} - {}", "scale = {}; {} * {}",
         "scale = {}; {} / {}", "scale = {}; {} % {}", "scale = {}; {} ^ {}",
         "{}k {} {} {}|pR", "scale = {}; sqrt({})", "scale = {}; e({})",
         "scale = {}; l({})", "scale = {}; a({})", "scale = {}; s({})",
         "scale = {}; c({})", "scale = {}; j({}, {})" ]

# Constants to make some code easier later.
div = 3
mod = 4
power = 5
modexp = 6
sqrt = 7
exponent = 8
bessel = 13

gen_ops = []
tests = []

# Infinite loop until the user sends SIGINT.
try:
	i = 0
	while True:
		run_test(i)
		i = i + 1
except KeyboardInterrupt:
	pass

# This is where we start processing the checklist of possible failures. Why only
# possible failures? Because some operations, specifically the functions in the
# math library, are not guaranteed to be exactly correct. Because of that, we
# need to present every failed test to the user for a final check before we
# add them as test cases.

# No items, just exit.
if len(tests) == 0:
	print("\nNo items in checklist.")
	print("Exiting")
	sys.exit(0)

print("\nGoing through the checklist...\n")

# Just do some error checking. If this fails here, it's a bug in this script.
if len(tests) != len(gen_ops):
	print("Corrupted checklist!")
	print("Exiting...")
	sys.exit(1)

# Go through each item in the checklist.
for i in range(0, len(tests)):

	# Yes, there's some code duplication. Sue me.

	print("\n{}".format(tests[i]))

	op = int(gen_ops[i])

	if op != modexp:
		exe = "bc"
		halt = "halt"
		options = "-lq"
	else:
		exe = "dc"
		halt = "q"
		options = ""

	# We want to run the test again to show the user the difference.
	indata = tests[i] + "\n" + halt

	args = [ exe, options ]

	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

	expected = p.stdout.decode()

	bcexe = exedir + "/" + exe
	args = [ bcexe, options ]

	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

	compare(exe, options, p, tests[i], halt, expected, op, False)

	# Ask the user to make a decision on the failed test.
	answer = input("\nAdd test ({}/{}) to test suite? [y/N]: ".format(i + 1, len(tests)))

	# Quick and dirty answer parsing.
	if 'Y' in answer or 'y' in answer:

		print("Yes")

		name = testdir + "/" + exe + "/" + files[op]

		# Write the test to the test file and the expected result to the
		# results file.
		with open(name + ".txt", "a") as f:
			f.write(tests[i] + "\n")

		with open(name + "_results.txt", "a") as f:
			f.write(expected)

	else:
		print("No")

print("Done!")
