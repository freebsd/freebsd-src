# $NetBSD: t_id.sh,v 1.1 2012/03/17 16:33:14 jruoho Exp $
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

create_run_id() {
	cat >run_id.sh <<EOF
#! /bin/sh
[ -f ./id ] || ln -s $(atf_get_srcdir)/h_id ./id
./id "\${@}"
EOF
	chmod +x run_id.sh
}

atf_test_case default
default_head() {
	atf_set "descr" "Checks that the output without options is correct"
}
default_body() {
	create_run_id

	echo "uid=100(test) gid=100(users) groups=100(users),0(wheel)" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh test

	echo "uid=0(root) gid=0(wheel) groups=0(wheel)" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh root

	export LIBFAKE_EGID_ROOT=1 LIBFAKE_EUID_ROOT=1
	echo "uid=100(test) gid=100(users) euid=0(root) egid=0(wheel) groups=100(users),0(wheel)" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh
	unset LIBFAKE_EGID_ROOT LIBFAKE_EUID_ROOT

	echo 'id: nonexistent: No such user' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_id.sh nonexistent

	atf_check -s eq:1 -o empty -e save:stderr ./run_id.sh root nonexistent
	atf_check -s eq:0 -o ignore -e empty grep ^usage: stderr
}

atf_test_case primaries
primaries_head() {
	atf_set "descr" "Checks that giving multiple primaries" \
	                "simultaneously provides an error"
}
primaries_body() {
	create_run_id

	for p1 in -G -g -p -u; do
		for p2 in -G -g -p -u; do
			if [ ${p1} != ${p2} ]; then
				atf_check -s eq:1 -o empty -e save:stderr \
				    ./run_id.sh ${p1} ${p2}
				atf_check -s eq:0 -o ignore -e empty \
				    grep ^usage: stderr
			fi
		done
	done
}

atf_test_case Gflag
Gflag_head() {
	atf_set "descr" "Checks that the -G primary flag works"
}
Gflag_body() {
	create_run_id

	echo "100 0" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G test

	echo "users wheel" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G -n
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G -n test

	echo "0" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G root

	echo "wheel" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G -n 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -G -n root

	echo 'id: nonexistent: No such user' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_id.sh -G nonexistent

	atf_check -s eq:1 -o empty -e save:stderr ./run_id.sh -G root nonexistent
	atf_check -s eq:0 -o ignore -e empty grep ^usage: stderr
}

atf_test_case gflag
gflag_head() {
	atf_set "descr" "Checks that the -g primary flag works"
}
gflag_body() {
	create_run_id

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g test

	echo "users" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -n
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -n test

	echo "0" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g root

	echo "wheel" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -n 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -n root

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r

	echo "users" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r -n

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r test

	echo "users" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r -n test

	export LIBFAKE_EGID_ROOT=1 LIBFAKE_EUID_ROOT=1

	echo "0" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g

	echo "wheel" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -n

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r

	echo "users" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r -n

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r test

	echo "users" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -g -r -n test

	unset LIBFAKE_EGID_ROOT LIBFAKE_EUID_ROOT

	echo 'id: nonexistent: No such user' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_id.sh -g nonexistent

	atf_check -s eq:1 -o empty -e save:stderr ./run_id.sh -g root nonexistent
	atf_check -s eq:0 -o ignore -e empty grep ^usage: stderr
}

atf_test_case pflag
pflag_head() {
	atf_set "descr" "Checks that the -p primary flag works"
}
pflag_body() {
	create_run_id

	cat >expout <<EOF
uid	test
groups	users wheel
EOF
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -p
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -p 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -p test

	cat >expout <<EOF
uid	root
groups	wheel
EOF
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -p 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -p root

	export LIBFAKE_EGID_ROOT=1 LIBFAKE_EUID_ROOT=1
	cat >expout <<EOF
uid	test
euid	root
rgid	users
groups	users wheel
EOF
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -p
	unset LIBFAKE_EGID_ROOT LIBFAKE_EUID_ROOT

	echo 'id: nonexistent: No such user' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_id.sh -p nonexistent

	atf_check -s eq:1 -o empty -e save:stderr ./run_id.sh -p root nonexistent
	atf_check -s eq:0 -o ignore -e empty grep ^usage: stderr
}

atf_test_case uflag
uflag_head() {
	atf_set "descr" "Checks that the -u primary flag works"
}
uflag_body() {
	create_run_id

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u test

	echo "test" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -n
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -n test

	echo "0" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u root

	echo "root" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -n 0
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -n root

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r

	echo "test" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r -n

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r test

	echo "test" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r -n test

	export LIBFAKE_EGID_ROOT=1 LIBFAKE_EUID_ROOT=1

	echo "0" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u

	echo "root" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -n

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r

	echo "test" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r -n

	echo "100" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r test

	echo "test" >expout
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r -n 100
	atf_check -s eq:0 -o file:expout -e empty ./run_id.sh -u -r -n test

	unset LIBFAKE_EGID_ROOT LIBFAKE_EUID_ROOT

	echo 'id: nonexistent: No such user' >experr
	atf_check -s eq:1 -o empty -e file:experr ./run_id.sh -u nonexistent

	atf_check -s eq:1 -o empty -e save:stderr \
	    ./run_id.sh -u root nonexistent
	atf_check -s eq:0 -o ignore -e empty grep ^usage: stderr
}

atf_init_test_cases()
{
	atf_add_test_case default
	atf_add_test_case primaries
	atf_add_test_case Gflag
	atf_add_test_case gflag
	atf_add_test_case pflag
	atf_add_test_case uflag
}
