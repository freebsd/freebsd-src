#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Rubicon Communications, LLC (Netgate)
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

. $(atf_get_srcdir)/utils.subr

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Test setting and retrieving debug level'
	atf_set require.user root
}

basic_body()
{
	pft_init

	vnet_mkjail debug
	atf_check -s exit:0 -e ignore \
	    jexec debug pfctl -x loud

	atf_check -s exit:0 -o match:'Debug: Loud' \
	    jexec debug pfctl -si
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "reset" "cleanup"
reset_head()
{
	atf_set descr 'Test resetting debug level'
	atf_set require.user root
}

reset_body()
{
	pft_init

	vnet_mkjail debug

	# Default is Urgent
	atf_check -s exit:0 -o match:'Debug: Urgent' \
	    jexec debug pfctl -sa
	state_limit=$(jexec debug pfctl -sa | grep 'states.*hard limit' | awk '{ print $4; }')

	# Change defaults
	pft_set_rules debug \
	    "set limit states 42"
	atf_check -s exit:0 -e ignore \
	    jexec debug pfctl -x loud

	atf_check -s exit:0 -o match:'Debug: Loud' \
	    jexec debug pfctl -sa
	new_state_limit=$(jexec debug pfctl -sa | grep 'states.*hard limit' | awk '{ print $4; }')
	if [ $state_limit -eq $new_state_limit ]; then
		jexec debug pfctl -sa
		atf_fail "Failed to change state limit"
	fi

	# Reset
	atf_check -s exit:0 -o ignore -e ignore \
	    jexec debug pfctl -FR
	atf_check -s exit:0 -o match:'Debug: Urgent' \
	    jexec debug pfctl -sa
	new_state_limit=$(jexec debug pfctl -sa | grep 'states.*hard limit' | awk '{ print $4; }')
	if [ $state_limit -ne $new_state_limit ]; then
		jexec debug pfctl -sa
		atf_fail "Failed to reset state limit"
	fi
}

reset_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "reset"
}
