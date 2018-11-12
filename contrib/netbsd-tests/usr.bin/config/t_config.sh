# $NetBSD: t_config.sh,v 1.1 2012/03/17 16:33:12 jruoho Exp $
#
# Copyright (c) 2008, 2010 The NetBSD Foundation, Inc.
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

run_and_check_pass()
{
	local name="${1}"; shift

	mkdir compile
	supportdir="$(atf_get_srcdir)/support"
	config="$(atf_get_srcdir)/d_${name}"

	atf_check -o ignore \
	    config -s "${supportdir}" -b "compile/${name}" "${config}"
}

run_and_check_fail()
{
	local name="${1}"; shift

	mkdir compile
	supportdir="$(atf_get_srcdir)/support"
	config="$(atf_get_srcdir)/d_${name}"

	atf_check -o ignore -e ignore -s ne:0 \
	    config -s "${supportdir}" -b "compile/${name}" "${config}"
}

# Defines a test case for config(1).
test_case()
{
	local name="${1}"; shift
	local type="${1}"; shift
	local descr="${*}"

	atf_test_case "${name}"
	eval "${name}_head() { \
		atf_set descr \"${descr}\"; \
		atf_set require.progs \"config\"; \
	}"
	eval "${name}_body() { \
		run_and_check_${type} '${name}'; \
	}"
}

test_case shadow_instance pass "Checks correct handling of shadowed instances"
test_case loop pass "Checks correct handling of loops"
test_case loop2 pass "Checks correct handling of devices that can be their" \
    "own parents"
test_case pseudo_parent pass "Checks correct handling of children of pseudo" \
    "devices (PR/32329)"
test_case postponed_orphan fail "Checks that config catches adding an" \
    "instance of a child of a negated instance as error"
test_case no_pseudo fail "Checks that config catches ommited 'pseudo-device'" \
    "as error (PR/34111)"
test_case deffs_redef fail "Checks that config doesn't allow a deffs to use" \
    "the same name as a previous defflag/defparam"

atf_init_test_cases()
{
	atf_add_test_case shadow_instance
	atf_add_test_case loop
	atf_add_test_case loop2
	atf_add_test_case pseudo_parent
	atf_add_test_case postponed_orphan
	atf_add_test_case no_pseudo
	atf_add_test_case deffs_redef
}
