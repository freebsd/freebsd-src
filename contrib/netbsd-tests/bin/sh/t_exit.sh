# $NetBSD: t_exit.sh,v 1.3 2012/04/13 06:12:32 jruoho Exp $
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

crud() {
	test yes = no

	cat <<EOF
$?
EOF
}

atf_test_case background
background_head() {
	atf_set "descr" "Tests that sh(1) sets '$?' properly when running " \
	                "a command in the background (PR bin/46327)"
}
background_body() {
	atf_check -s exit:0 -o ignore -e ignore -x "true; true & echo $?"
	atf_check -s exit:0 -o ignore -e ignore -x "false; true & echo $?"
}

atf_test_case function
function_head() {
	atf_set "descr" "Tests that \$? is correctly updated inside" \
	                "a function"
}
function_body() {
	foo=`crud`
	atf_check_equal 'x$foo' 'x1'
}

atf_test_case readout
readout_head() {
	atf_set "descr" "Tests that \$? is correctly updated in a" \
	                "compound expression"
}
readout_body() {
	atf_check_equal '$( true && ! true | false; echo $? )' '0'
}

atf_test_case trap_subshell
trap_subshell_head() {
	atf_set "descr" "Tests that the trap statement in a subshell" \
	    "works when the subshell exits"
}
trap_subshell_body() {
	atf_check -s eq:0 -o inline:'exiting\n' -x \
	    '( trap "echo exiting" EXIT; /usr/bin/true )'
}

atf_test_case trap_zero__implicit_exit
trap_zero__implicit_exit_body() {
	# PR bin/6764: sh works but ksh does not"
	echo '( trap "echo exiting" 0 )' >helper.sh
	atf_check -s eq:0 -o match:exiting -e empty /bin/sh helper.sh
	atf_check -s eq:0 -o match:exiting -e empty /bin/ksh helper.sh
}

atf_test_case trap_zero__explicit_exit
trap_zero__explicit_exit_body() {
	echo '( trap "echo exiting" 0; exit )' >helper.sh
	atf_check -s eq:0 -o match:exiting -e empty /bin/sh helper.sh
	atf_check -s eq:0 -o match:exiting -e empty /bin/ksh helper.sh
}

atf_test_case trap_zero__explicit_return
trap_zero__explicit_return_body() {
	echo '( trap "echo exiting" 0; return )' >helper.sh
	atf_check -s eq:0 -o match:exiting -e empty /bin/sh helper.sh
	atf_check -s eq:0 -o match:exiting -e empty /bin/ksh helper.sh
}

atf_init_test_cases() {
	atf_add_test_case background
	atf_add_test_case function
	atf_add_test_case readout
	atf_add_test_case trap_subshell
	atf_add_test_case trap_zero__implicit_exit
	atf_add_test_case trap_zero__explicit_exit
	atf_add_test_case trap_zero__explicit_return
}
