# $NetBSD: t_whoami.sh,v 1.1 2012/03/17 16:33:14 jruoho Exp $
#
# Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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

create_run_whoami() {
	cat >run_whoami.sh <<EOF
#! /bin/sh
[ -f ./whoami ] || ln -s $(atf_get_srcdir)/h_id ./whoami
./whoami "\${@}"
EOF
	chmod +x run_whoami.sh
}

atf_test_case correct
correct_head() {
	atf_set "descr" "Checks that correct queries work"
}
correct_body() {
	create_run_whoami

	echo "Checking with EUID=100"
	echo "test" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_whoami.sh

	echo "Checking with EUID=0"
	export LIBFAKE_EUID_ROOT=1
	echo "root" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_whoami.sh
}

atf_test_case syntax
syntax_head() {
	atf_set "descr" "Checks the command's syntax"
}
syntax_body() {
	create_run_whoami

	# Give a user to the command.
	echo 'usage: whoami' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_whoami.sh root

	# Give an invalid flag but which is allowed by id (with which
	# whoami shares code) when using the -un options.
	echo 'usage: whoami' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_whoami.sh -r
}

atf_init_test_cases()
{
	atf_add_test_case correct
	atf_add_test_case syntax
}
