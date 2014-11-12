# $NetBSD: t_compexit.sh,v 1.1 2012/03/17 16:33:11 jruoho Exp $
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

# The standard
# http://www.opengroup.org/onlinepubs/007904975/utilities/set.html
# says:
#
# -e
#
# When this option is on, if a simple command fails for any of the
# reasons listed in Consequences of Shell Errors or returns an exit
# status value >0, and is not part of the compound list following a
# while, until, or if keyword, and is not a part of an AND or OR list,
# and is not a pipeline preceded by the !  reserved word, then the shell
# shall immediately exit.

crud() {
	set -e
	for x in a
	do
		BAR="foo"
		false && echo true
		echo mumble
	done
}

atf_test_case set_e
set_e_head() {
	atf_set "descr" "Tests that 'set -e' turns on error detection" \
	                "and that it behaves as defined by the standard"
}
set_e_body() {
	foo=`crud`
	atf_check_equal 'x$foo' 'xmumble'
}

atf_init_test_cases() {
	atf_add_test_case set_e
}
