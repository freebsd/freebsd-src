#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2016 Dell EMC
# All rights reserved.
# Copyright (c) 2025 Klara, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

SRCDIR=$(atf_get_srcdir)

check()
{
	local tc=${1}
	local profile_flag

	cp "${SRCDIR}/${tc%.[0-9]}".* .

	if [ -f "${tc}.pro" ]; then
		profile_flag="-P${tc}.pro"
	else
		# Make sure we don't implicitly use ~/.indent.pro from the test
		# host, for determinism purposes.
		profile_flag="-npro"
	fi
	atf_check -s exit:${tc##*.} -o file:"${tc}.stdout" \
	    indent ${profile_flag} < "${tc}"
}

add_legacy_testcase()
{
	local tc=${1}

	atf_test_case ${tc%.[0-9]}
	eval "${tc%.[0-9]}_body() { check ${tc}; }"
	atf_add_test_case ${tc%.[0-9]}
}

atf_test_case backup_suffix
backup_suffix_body()
{
	local argmax=$(sysctl -n kern.argmax)
	local suffix=$(jot -b .bak -s '' $((argmax/5)))
	local code=$'int main() {}\n'

	printf "${code}" >input.c

	atf_check indent input.c
	atf_check -o inline:"${code}" cat input.c.BAK

	atf_check -s exit:1 -e match:"name too long"\
	    env SIMPLE_BACKUP_SUFFIX=${suffix} indent input.c
}

atf_init_test_cases()
{
	for tc in $(find -s "${SRCDIR}" -name '*.[0-9]'); do
		add_legacy_testcase "${tc##*/}"
	done
	atf_add_test_case backup_suffix
}
