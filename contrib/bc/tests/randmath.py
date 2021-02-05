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

import os, errno
import random
import sys
import subprocess

def gen(limit=4):
	return random.randint(0, 2 ** (8 * limit))

def negative():
	return random.randint(0, 1) == 1

def zero():
	return random.randint(0, 2 ** (8) - 1) == 0

def num(op, neg, real, z, limit=4):

	if z:
		z = zero()
	else:
		z = False

	if z:
		return 0

	if neg:
		neg = negative()

	g = gen(limit)

	if real and negative():
		n = str(gen(25))
		length = gen(7 / 8)
		if len(n) < length:
			n = ("0" * (length - len(n))) + n
	else:
		n = "0"

	g = str(g)
	if n != "0":
		g = g + "." + n

	if neg and g != "0":
		if op != modexp:
			g = "-" + g
		else:
			g = "_" + g

	return g


def add(test, op):

	tests.append(test)
	gen_ops.append(op)

def compare(exe, options, p, test, halt, expected, op, do_add=True):

	if p.returncode != 0:

		print("    {} returned an error ({})".format(exe, p.returncode))

		if do_add:
			print("    adding to checklist...")
			add(test, op)

		return

	actual = p.stdout.decode()

	if actual != expected:

		if op >= exponent:

			indata = "scale += 10; {}; {}".format(test, halt)
			args = [ exe, options ]
			p2 = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			expected = p2.stdout[:-10].decode()

			if actual == expected:
				print("    failed because of bug in other {}".format(exe))
				print("    continuing...")
				return

		if do_add:
			print("   failed; adding to checklist...")
			add(test, op)
		else:
			print("   failed {}".format(test))
			print("    expected:")
			print("        {}".format(expected))
			print("    actual:")
			print("        {}".format(actual))


def gen_test(op):

	scale = num(op, False, False, True, 5 / 8)

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

def run_test(t):

	op = random.randrange(bessel + 1)

	if op != modexp:
		exe = "bc"
		halt = "halt"
		options = "-lq"
	else:
		exe = "dc"
		halt = "q"
		options = ""

	test = gen_test(op)

	if "c(0)" in test or "scale = 4; j(4" in test:
		return

	bcexe = exedir + "/" + exe
	indata = test + "\n" + halt

	print("Test {}: {}".format(t, test))

	if exe == "bc":
		args = [ exe, options ]
	else:
		args = [ exe ]

	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

	output1 = p.stdout.decode()

	if p.returncode != 0 or output1 == "":
		print("    other {} returned an error ({}); continuing...".format(exe, p.returncode))
		return

	if output1 == "\n":
		print("   other {} has a bug; continuing...".format(exe))
		return

	if output1 == "-0\n":
		output1 = "0\n"
	elif output1 == "-0":
		output1 = "0"

	args = [ bcexe, options ]

	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	compare(exe, options, p, test, halt, output1, op)


if __name__ != "__main__":
	sys.exit(1)

script = sys.argv[0]
testdir = os.path.dirname(script)

exedir = testdir + "/../bin"

ops = [ '+', '-', '*', '/', '%', '^', '|' ]
files = [ "add", "subtract", "multiply", "divide", "modulus", "power", "modexp",
          "sqrt", "exponent", "log", "arctangent", "sine", "cosine", "bessel" ]
funcs = [ "sqrt", "e", "l", "a", "s", "c", "j" ]

fmts = [ "scale = {}; {} + {}", "scale = {}; {} - {}", "scale = {}; {} * {}",
         "scale = {}; {} / {}", "scale = {}; {} % {}", "scale = {}; {} ^ {}",
         "{}k {} {} {}|pR", "scale = {}; sqrt({})", "scale = {}; e({})",
         "scale = {}; l({})", "scale = {}; a({})", "scale = {}; s({})",
         "scale = {}; c({})", "scale = {}; j({}, {})" ]

div = 3
mod = 4
power = 5
modexp = 6
sqrt = 7
exponent = 8
bessel = 13

gen_ops = []
tests = []

try:
	i = 0
	while True:
		run_test(i)
		i = i + 1
except KeyboardInterrupt:
	pass

if len(tests) == 0:
	print("\nNo items in checklist.")
	print("Exiting")
	sys.exit(0)

print("\nGoing through the checklist...\n")

if len(tests) != len(gen_ops):
	print("Corrupted checklist!")
	print("Exiting...")
	sys.exit(1)

for i in range(0, len(tests)):

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

	indata = tests[i] + "\n" + halt

	args = [ exe, options ]

	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

	expected = p.stdout.decode()

	bcexe = exedir + "/" + exe
	args = [ bcexe, options ]

	p = subprocess.run(args, input=indata.encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

	compare(exe, options, p, tests[i], halt, expected, op, False)

	answer = input("\nAdd test ({}/{}) to test suite? [y/N]: ".format(i + 1, len(tests)))

	if 'Y' in answer or 'y' in answer:

		print("Yes")

		name = testdir + "/" + exe + "/" + files[op]

		with open(name + ".txt", "a") as f:
			f.write(tests[i] + "\n")

		with open(name + "_results.txt", "a") as f:
			f.write(expected)

	else:
		print("No")

print("Done!")
