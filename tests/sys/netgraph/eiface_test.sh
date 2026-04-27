#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Zhenlei Huang <zlei@FreeBSD.org>
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

. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "pr292993" "cleanup"
pr292993_head()
{
	atf_set descr 'Test for PR 292993'
	atf_set require.user root
}

pr292993_body()
{
	vnet_init

	for i in `seq 1 10`
	do
		ngeth=$(ngctl -f - <<__EOF__ | awk '$1 == "Args:" {print substr($2, 2, length($2)-2)}')
mkpeer . eiface path_$i ether
msg .path_$i getifname
__EOF__
		# Sanity check
		atf_check -s exit:0 -o ignore \
			ifconfig $ngeth
		jail -c vnet name="eiface_destroy_$i" path=/ \
		    vnet.interface="$ngeth" exec.start="sleep 0.1" &
		pid=$!
		sleep 0.1
		ngctl shutdown ${ngeth}:
		wait $pid
	done
	true
}

pr292993_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "pr292993"
}
