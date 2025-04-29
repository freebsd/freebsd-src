#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
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

# See regression fixed in baad45c9c12028964acd0b58096f3aaa0fb22859
atf_test_case "IP_MULTICAST_IF" "cleanup"
IP_MULTICAST_IF_head()
{
	atf_set descr \
	    'sendto() for IP_MULTICAST_IF socket does not do routing lookup'
	atf_set require.user root

}

IP_MULTICAST_IF_body()
{
	local epair mjail

	vnet_init
	# The test doesn't use our half of epair
	epair=$(vnet_mkepair)
	vnet_mkjail mjail ${epair}a
	jexec mjail ifconfig ${epair}a up
	jexec mjail ifconfig ${epair}a 192.0.2.1/24
	atf_check -s exit:0 -o empty \
	    jexec mjail $(atf_get_srcdir)/sendto-IP_MULTICAST_IF 192.0.2.1
}

IP_MULTICAST_IF_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "IP_MULTICAST_IF"
}
