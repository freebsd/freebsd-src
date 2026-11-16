#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Avinash Duduskar <avinash.duduskar@gmail.com>
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

atf_test_case "pfctl_adopt" "cleanup"
pfctl_adopt_head()
{
	atf_set descr 'pfctl -l across a privilege drop, as authpf(8) runs it'
	atf_set require.user root
	atf_set require.progs python3
}

pfctl_adopt_body()
{
	pft_init

	# pfctl is handed /dev/pf as -p /dev/fd/N, which needs fdescfs.
	if ! ( exec 3</dev/null; [ -e /dev/fd/3 ] ); then
		atf_skip "This test requires fdescfs mounted on /dev/fd"
	fi

	helper=$(atf_get_srcdir)/pfctl_adopt.py

	vnet_mkjail alcatraz
	jexec alcatraz pfctl -e

	# Without -l pfctl opens its own, unprivileged socket (PR 291981).
	echo "pass" | atf_check -s not-exit:0 -e ignore \
	    jexec alcatraz ${helper} -a authpf_test -f -
	atf_check -s exit:0 -o not-match:"authpf_test" \
	    jexec alcatraz pfctl -q -sA

	# An unusable descriptor is an error, not a fallback to a new socket.
	atf_check -s exit:1 -e ignore \
	    jexec alcatraz sh -c 'pfctl -l 3 -sr 3</dev/null'

	echo "pass" | atf_check -s exit:0 \
	    jexec alcatraz ${helper} -l -a authpf_test -f -
	atf_check -s exit:0 -o match:"^pass all" \
	    jexec alcatraz pfctl -q -a authpf_test -sr
}

pfctl_adopt_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "pfctl_adopt"
}
