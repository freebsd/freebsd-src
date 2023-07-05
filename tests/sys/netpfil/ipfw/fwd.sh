# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Gleb Smirnoff <glebius@FreeBSD.org>
# Copyright (c) 2022 Pavel Polyakov <bsd@kobyla.org>
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

. $(atf_get_srcdir)/../common/utils.subr

atf_test_case "local" "cleanup"
local_head()
{
	atf_set descr 'Test that fwd 127.0.0.1,port works'
	atf_set require.user root
}

local_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b

	ifconfig ${epair}a 192.0.2.0/31 up
	route add 192.0.2.3/32 192.0.2.1

	jexec alcatraz ifconfig lo0 127.0.0.1/8 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/31 up
	jexec alcatraz route add default 192.0.2.0
	jexec alcatraz /usr/sbin/inetd -p /dev/null $(atf_get_srcdir)/fwd_inetd.conf

	firewall_config alcatraz ipfw ipfw \
	    "ipfw add 10 fwd 127.0.0.1,82 tcp from any to any dst-port 80 in via ${epair}b" \
	    "ipfw add 20 allow all from any to any"

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.1

	reply=$(nc -nN 192.0.2.3 80 < /dev/null)
	atf_check [ "${reply}" = "GOOD 82" ]
}

local_cleanup()
{
	firewall_cleanup $1
}

atf_init_test_cases()
{
	atf_add_test_case "local"
}
