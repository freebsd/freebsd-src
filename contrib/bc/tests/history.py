#! /usr/bin/python
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

import os, sys
import time
import signal
import traceback

try:
	import pexpect
except ImportError:
	print("Could not find pexpect. Skipping...")
	sys.exit(0)

# Housekeeping.
script = sys.argv[0]
testdir = os.path.dirname(script)

if "BC_TEST_OUTPUT_DIR" in os.environ:
	outputdir = os.environ["BC_TEST_OUTPUT_DIR"]
else:
	outputdir = testdir

prompt = ">>> "

# This array is for escaping characters that are necessary to escape when
# outputting to pexpect. Since pexpect takes regexes, these characters confuse
# it unless we escape them.
escapes = [
	']',
	'[',
	'+',
]

# UTF-8 stress tests.
utf8_stress1 = "ᆬḰ䋔䗅㜲ತ咡䒢岤䳰稨⣡嶣㷡嶏ⵐ䄺嵕ਅ奰痚㆜䊛拂䅙૩➋䛿ቬ竳Ϳᅠ❄产翷䮊௷Ỉ䷒䳜㛠➕傎ᗋᏯਕ䆐悙癐㺨"
utf8_stress2 = "韠싧돳넨큚ꉿ뮴픷ꉲ긌�최릙걆鳬낽ꪁ퍼鈴핐黙헶ꪈ뮩쭀锻끥鉗겉욞며뛯꬐�ﻼ�度錐�"
utf8_stress3 = "곻�䣹昲蜴Ὓ桢㎏⚦珢畣갴ﭱ鶶ๅ⶛뀁彻ꖒ䔾ꢚﱤ햔햞㐹�鼳뵡▿ⶾ꠩�纞⊐佧�ⵟ霘紳㱔籠뎼⊓搧硤"
utf8_stress4 = "ᄀ𖢾🏴��"

# An easy array for UTF-8 tests.
utf8_stress_strs = [
	utf8_stress1,
	utf8_stress2,
	utf8_stress3,
	utf8_stress4,
]


def expect(child, data):
	child.expect(data)


# Eats all of the child's data.
# @param child  The child whose data should be eaten.
def eat(child):
	while child.buffer is not None and len(child.buffer) > 0:
		expect(child, ".+")


# Send data to a child. This makes sure the buffers are empty first.
# @param child  The child to send data to.
# @param data   The data to send.
def send(child, data):
	eat(child)
	child.send(data)


def wait(child):
	if child.isalive():
		child.sendeof()
		time.sleep(1)
		if child.isalive():
			child.kill(signal.SIGTERM)
			time.sleep(1)
			if child.isalive():
				child.kill(signal.SIGKILL)
	child.wait()


# Check that the child output the expected line. If history is false, then
# the output should change.
def check_line(child, expected, prompt=">>> ", history=True):
	send(child, "\n")
	prefix = "\r\n" if history else ""
	expect(child, prefix + expected + "\r\n" + prompt)


# Write a string to output, checking all of the characters are output,
# one-by-one.
def write_str(child, s):
	for c in s:
		send(child, c)
		if c in escapes:
			expect(child, "\\{}".format(c))
		else:
			expect(child, c)


# Check the bc banner.
# @param child  The child process.
def bc_banner(child):
	bc_banner1 = "bc [0-9]+\.[0-9]+\.[0-9]+\r\n"
	bc_banner2 = "Copyright \(c\) 2018-[2-9][0-9][0-9][0-9] Gavin D. Howard and contributors\r\n"
	bc_banner3 = "Report bugs at: https://git.yzena.com/gavin/bc\r\n\r\n"
	bc_banner4 = "This is free software with ABSOLUTELY NO WARRANTY.\r\n\r\n"
	expect(child, bc_banner1)
	expect(child, bc_banner2)
	expect(child, bc_banner3)
	expect(child, bc_banner4)
	expect(child, prompt)


# Common UTF-8 testing function. The index is the index into utf8_stress_strs
# for which stress string to use.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
# @param idx   The index of the UTF-8 stress string.
def test_utf8(exe, args, env, idx, bc=True):

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	child = pexpect.spawn(exe, args=args, env=env, encoding='utf-8', codec_errors='ignore')

	try:

		# Write the stress string.
		send(child, utf8_stress_strs[idx])
		send(child, "\n")

		if bc:
			send(child, "quit")
		else:
			send(child, "q")

		send(child, "\n")

		wait(child)

	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child

# A random UTF-8 test with insert.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_utf8_0(exe, args, env, bc=True):

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	child = pexpect.spawn(exe, args=args, env=env, encoding='utf-8', codec_errors='ignore')

	try:

		# Just random UTF-8 I generated somewhow, plus ensuring that insert works.
		write_str(child, "ﴪáá̵̗🈐ã")
		send(child, "\x1b[D\x1b[D\x1b[D\x1b\x1b[Aℐ")
		send(child, "\n")

		if bc:
			send(child, "quit")
		else:
			send(child, "q")

		send(child, "\n")
		eat(child)

		wait(child)

	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


def test_utf8_1(exe, args, env, bc=True):
	return test_utf8(exe, args, env, 0, bc)


def test_utf8_2(exe, args, env, bc=True):
	return test_utf8(exe, args, env, 1, bc)


def test_utf8_3(exe, args, env, bc=True):
	return test_utf8(exe, args, env, 2, bc)


def test_utf8_4(exe, args, env, bc=True):
	return test_utf8(exe, args, env, 3, bc)


# This tests a SIGINT with reset followed by a SIGQUIT.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_sigint_sigquit(exe, args, env):

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		send(child, "\t")
		expect(child, "        ")
		send(child, "\x03")
		send(child, "\x1c")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Test for EOF.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_eof(exe, args, env):

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		send(child, "\t")
		expect(child, "        ")
		send(child, "\x04")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Test for quiting SIGINT.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_sigint(exe, args, env):

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	env["BC_SIGINT_RESET"] = "0"
	env["DC_SIGINT_RESET"] = "0"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		send(child, "\t")
		expect(child, "        ")
		send(child, "\x03")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Test for SIGTSTP.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_sigtstp(exe, args, env):

	# This test does not work on FreeBSD, so skip.
	if sys.platform.startswith("freebsd"):
		sys.exit(0)

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		send(child, "\t")
		expect(child, "        ")
		send(child, "\x13")
		time.sleep(1)
		if not child.isalive():
			print("child exited early")
			print(str(child))
			print(str(child.buffer))
			sys.exit(1)
		child.kill(signal.SIGCONT)
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Test for SIGSTOP.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_sigstop(exe, args, env):

	# Because both bc and dc use this, make sure the banner doesn't pop.
	env["BC_BANNER"] = "0"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		send(child, "\t")
		expect(child, "        ")
		send(child, "\x14")
		time.sleep(1)
		if not child.isalive():
			print("child exited early")
			print(str(child))
			print(str(child.buffer))
			sys.exit(1)
		send(child, "\x13")
		time.sleep(1)
		if not child.isalive():
			print("child exited early")
			print(str(child))
			print(str(child.buffer))
			sys.exit(1)
		child.kill(signal.SIGCONT)
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


def test_bc_utf8_0(exe, args, env):
	return test_utf8_0(exe, args, env, True)


def test_bc_utf8_1(exe, args, env):
	return test_utf8_1(exe, args, env, True)


def test_bc_utf8_2(exe, args, env):
	return test_utf8_2(exe, args, env, True)


def test_bc_utf8_3(exe, args, env):
	return test_utf8_3(exe, args, env, True)


def test_bc_utf8_4(exe, args, env):
	return test_utf8_4(exe, args, env, True)


# Basic bc test.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc1(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		write_str(child, "1")
		check_line(child, "1")
		write_str(child, "1")
		check_line(child, "1")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# SIGINT with no history.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc2(exe, args, env):

	env["TERM"] = "dumb"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		child.sendline("1")
		check_line(child, "1", history=False)
		time.sleep(1)
		child.sendintr()
		child.sendline("quit")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Left and right arrows.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc3(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x1b[D\x1b[D\x1b[C\x1b[C")
		send(child, "\n")
		expect(child, prompt)
		send(child, "12\x1b[D3\x1b[C4\x1bOD5\x1bOC6")
		send(child, "\n")
		check_line(child, "132546")
		send(child, "12\x023\x064")
		send(child, "\n")
		check_line(child, "1324")
		send(child, "12\x1b[H3\x1bOH\x01\x1b[H45\x1bOF6\x05\x1b[F7\x1bOH8")
		send(child, "\n")
		check_line(child, "84531267")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Up and down arrows.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc4(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x1b[A\x1bOA\x1b[B\x1bOB")
		send(child, "\n")
		expect(child, prompt)
		write_str(child, "15")
		check_line(child, "15")
		write_str(child, "2^16")
		check_line(child, "65536")
		send(child, "\x1b[A\x1bOA")
		send(child, "\n")
		check_line(child, "15")
		send(child, "\x1b[A\x1bOA\x1b[A\x1b[B")
		check_line(child, "65536")
		send(child, "\x1b[A\x1bOA\x0e\x1b[A\x1b[A\x1b[A\x1b[B\x10\x1b[B\x1b[B\x1bOB\x1b[B\x1bOA")
		send(child, "\n")
		check_line(child, "65536")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Clear screen.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc5(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x0c")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Printed material without a newline.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc6(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "print \"Enter number: \"")
		send(child, "\n")
		expect(child, "Enter number: ")
		send(child, "4\x1b[A\x1b[A")
		send(child, "\n")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Word start and word end.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc7(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x1bb\x1bb\x1bf\x1bf")
		send(child, "\n")
		expect(child, prompt)
		send(child, "\x1b[0~\x1b[3a")
		send(child, "\n")
		expect(child, prompt)
		send(child, "\x1b[0;4\x1b[0A")
		send(child, "\n")
		expect(child, prompt)
		send(child, "        ")
		send(child, "\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb")
		send(child, "\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf")
		send(child, "\n")
		expect(child, prompt)
		write_str(child, "12 + 34 + 56 + 78 + 90")
		check_line(child, "270")
		send(child, "\x1b[A")
		send(child, "\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb\x1bb")
		send(child, "\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf\x1bf")
		check_line(child, "270")
		send(child, "\x1b[A")
		send(child, "\x1bh\x1bh\x1bf + 14 ")
		send(child, "\n")
		check_line(child, "284")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Backspace.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc8(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "12\x1b[D3\x1b[C4\x08\x7f")
		send(child, "\n")
		check_line(child, "13")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Backspace and delete words.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc9(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x1b[0;5D\x1b[0;5D\x1b[0;5D\x1b[0;5C\x1b[0;5D\x1bd\x1b[3~\x1b[d\x1b[d\x1b[d\x1b[d\x7f\x7f\x7f")
		send(child, "\n")
		expect(child, prompt)
		write_str(child, "12 + 34 + 56 + 78 + 90")
		check_line(child, "270")
		send(child, "\x1b[A")
		send(child, "\x1b[0;5D\x1b[0;5D\x1b[0;5D\x1b[0;5C\x1b[0;5D\x1bd\x1b[3~\x1b[d\x1b[d\x1b[d\x1b[d\x7f\x7f\x7f")
		send(child, "\n")
		check_line(child, "102")
		send(child, "\x1b[A")
		send(child, "\x17\x17")
		send(child, "\n")
		check_line(child, "46")
		send(child, "\x17\x17")
		send(child, "\n")
		expect(child, prompt)
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Backspace and delete words 2.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc10(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x1b[3~\x1b[3~")
		send(child, "\n")
		expect(child, prompt)
		send(child, "    \x1b[3~\x1b[3~")
		send(child, "\n")
		expect(child, prompt)
		write_str(child, "12 + 34 + 56 + 78 + 90")
		check_line(child, "270")
		send(child, "\x1b[A\x1b[A\x1b[A\x1b[B\x1b[B\x1b[B\x1b[A")
		send(child, "\n")
		check_line(child, "270")
		send(child, "\x1b[A\x1b[0;5D\x1b[0;5D\x0b")
		send(child, "\n")
		check_line(child, "180")
		send(child, "\x1b[A\x1521")
		check_line(child, "21")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Swap.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc11(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "\x1b[A\x02\x14")
		send(child, "\n")
		expect(child, prompt)
		write_str(child, "12 + 34 + 56 + 78")
		check_line(child, "180")
		send(child, "\x1b[A\x02\x14")
		check_line(child, "189")
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Non-fatal error.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_bc12(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		bc_banner(child)
		send(child, "12 +")
		send(child, "\n")
		time.sleep(1)
		if not child.isalive():
			print("child exited early")
			print(str(child))
			print(str(child.buffer))
			sys.exit(1)
		send(child, "quit")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


def test_dc_utf8_0(exe, args, env):
	return test_utf8_0(exe, args, env, False)


def test_dc_utf8_1(exe, args, env):
	return test_utf8_1(exe, args, env, False)


def test_dc_utf8_2(exe, args, env):
	return test_utf8_2(exe, args, env, False)


def test_dc_utf8_3(exe, args, env):
	return test_utf8_3(exe, args, env, False)


def test_dc_utf8_4(exe, args, env):
	return test_utf8_4(exe, args, env, False)


# Basic dc test.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_dc1(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		write_str(child, "1pR")
		check_line(child, "1")
		write_str(child, "1pR")
		check_line(child, "1")
		write_str(child, "q")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# SIGINT with quit.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_dc2(exe, args, env):

	env["TERM"] = "dumb"

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		child.sendline("1pR")
		check_line(child, "1", history=False)
		time.sleep(1)
		child.sendintr()
		child.sendline("q")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# Execute string.
# @param exe   The executable.
# @param args  The arguments to pass to the executable.
# @param env   The environment.
def test_dc3(exe, args, env):

	child = pexpect.spawn(exe, args=args, env=env)

	try:
		write_str(child, "[1 15+pR]x")
		check_line(child, "16")
		write_str(child, "1pR")
		check_line(child, "1")
		write_str(child, "q")
		send(child, "\n")
		wait(child)
	except pexpect.TIMEOUT:
		traceback.print_tb(sys.exc_info()[2])
		print("timed out")
		print(str(child))
		sys.exit(2)
	except pexpect.EOF:
		print("EOF")
		print(str(child))
		print(str(child.buffer))
		print(str(child.before))
		sys.exit(2)

	return child


# The array of bc tests.
bc_tests = [
	test_bc_utf8_0,
	test_bc_utf8_1,
	test_bc_utf8_2,
	test_bc_utf8_3,
	test_bc_utf8_4,
	test_sigint_sigquit,
	test_eof,
	test_sigint,
	test_sigtstp,
	test_sigstop,
	test_bc1,
	test_bc2,
	test_bc3,
	test_bc4,
	test_bc5,
	test_bc6,
	test_bc7,
	test_bc8,
	test_bc9,
	test_bc10,
	test_bc11,
	test_bc12,
]

# The array of dc tests.
dc_tests = [
	test_dc_utf8_0,
	test_dc_utf8_1,
	test_dc_utf8_2,
	test_dc_utf8_3,
	test_sigint_sigquit,
	test_eof,
	test_sigint,
	test_dc1,
	test_dc2,
	test_dc3,
]


# Print the usage and exit with an error.
def usage():
	print("usage: {} [-t] dir [-a] test_idx [exe options...]".format(script))
	print("       The valid values for dir are: 'bc' and 'dc'.")
	print("       The max test_idx for bc is {}.".format(len(bc_tests) - 1))
	print("       The max test_idx for dc is {}.".format(len(dc_tests) - 1))
	print("       If -a is given, the number of tests for dir is printed.")
	print("       No tests are run.")
	sys.exit(1)


# Must run this script alone.
if __name__ != "__main__":
	usage()

if len(sys.argv) < 2:
	usage()

idx = 1

exedir = sys.argv[idx]

idx += 1

if exedir == "-t":
	do_test = True
	exedir = sys.argv[idx]
	idx += 1
else:
	do_test = False

test_idx = sys.argv[idx]

idx += 1

if test_idx == "-a":
	if exedir == "bc":
		l = len(bc_tests)
	else:
		l = len(dc_tests)
	print("{}".format(l))
	sys.exit(0)

test_idx = int(test_idx)

# Set a default executable unless we have one.
if len(sys.argv) >= idx + 1:
	exe = sys.argv[idx]
else:
	exe = testdir + "/../bin/" + exedir

exebase = os.path.basename(exe)

# Use the correct options.
if exebase == "bc":
	halt = "halt\n"
	options = "-l"
	test_array = bc_tests
else:
	halt = "q\n"
	options = "-x"
	test_array = dc_tests

# More command-line processing.
if len(sys.argv) > idx + 1:
	exe = [ exe, sys.argv[idx + 1:], options ]
else:
	exe = [ exe, options ]

# This is the environment necessary for most tests.
env = {
	"BC_BANNER": "1",
	"BC_PROMPT": "1",
	"DC_PROMPT": "1",
	"BC_TTY_MODE": "1",
	"DC_TTY_MODE": "1",
	"BC_SIGINT_RESET": "1",
	"DC_SIGINT_RESET": "1",
}

# Make sure to include the outside environment.
env.update(os.environ)
env.pop("BC_ENV_ARGS", None)
env.pop("BC_LINE_LENGTH", None)
env.pop("DC_ENV_ARGS", None)
env.pop("DC_LINE_LENGTH", None)

# Run the correct test.
child = test_array[test_idx](exe[0], exe[1:], env)

child.close()

exit = child.exitstatus

if exit is not None and exit != 0:
	print("child failed; expected exit code 0, got {}".format(exit))
	print(str(child))
	sys.exit(1)
