#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Dag-Erling Smørgrav <des@FreeBSD.org>
# Copyright (c) 2023 The FreeBSD Foundation
#
# This software was developed by Yan-Hao Wang <bses30074@gmail.com>
# under sponsorship from the FreeBSD Foundation.
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE
#

atf_test_case basic
basic_head()
{
	atf_set "descr" "Check that all reported variables " \
		"exist with the reported values."
}
basic_body()
{
	atf_check -o save:out printenv
	while IFS= read -r env; do
		env_name=${env%%=*}
		env_value=${env#*=}
		atf_check -o inline:"${env_value}" -x \
		    printf "%s" "\"\$${env_name}\""
	done <out
}

atf_test_case add_delete_env
add_delete_env_head()
{
	atf_set "descr" "New changes to the environment " \
		"should be reflected in printenv's output"
}
add_delete_env_body()
{
	env_name=$(date +"env_%Y%m%d%H%M%S")
	export "${env_name}=value"
	atf_check -o inline:"value\n" printenv "${env_name}"
	unset "${env_name}"
	atf_check -s exit:1 printenv "${env_name}"
}

atf_test_case multi
multi_head()
{
	atf_set "descr" "Error out if given multiple arguments"
}
multi_body()
{
	atf_check -s exit:1 -e match:usage printenv one two
}

atf_test_case stdout
stdout_head()
{
	atf_set "descr" "Failure to write to stdout"
}
stdout_body()
{
	local dir=$(atf_get_srcdir)
	(
		trap "" PIPE
		sleep 1
		printenv 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"1\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case add_delete_env
	atf_add_test_case multi
	atf_add_test_case stdout
}
