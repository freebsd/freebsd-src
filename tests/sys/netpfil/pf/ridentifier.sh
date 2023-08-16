#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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
	atf_set descr 'Test ridentifier keyword'
	atf_set require.user root
}

basic_body()
{
	pft_init
	pflog_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig lo0 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz /usr/sbin/inetd -p inetd-alcatraz.pid $(atf_get_srcdir)/echo_inetd.conf

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	jexec alcatraz pfctl -e
	jexec alcatraz ifconfig pflog0 up
	pft_set_rules alcatraz \
		"pass in log" \
		"pass in log proto tcp ridentifier 1234"

	jexec alcatraz tcpdump --immediate-mode -n -e -i pflog0 > tcpdump.log &
	sleep 1

	echo "test" | nc -N 192.0.2.2 7
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	sleep 1
	jexec alcatraz killall tcpdump

	# Make sure we spotted the ridentifier
	atf_check -s exit:0 -o ignore \
	    grep 'rule 1/0.*ridentifier 1234' tcpdump.log
	# But not on the !TCP traffic
	atf_check -s exit:1 -o ignore \
	    grep 'rule 0/0.*ridentifier' tcpdump.log

	# Now try with antispoof rules
	pft_set_rules alcatraz \
		"pass in log" \
		"antispoof log for ${epair}b ridentifier 4321"

	jexec alcatraz tcpdump --immediate-mode -n -e -i pflog0 > tcpdump.log &
	sleep 1

	# Without explicit rules for lo0 we're going to drop packets to ourself
	atf_check -s exit:2 -o ignore -e ignore \
	    jexec alcatraz ping -c 1 -t 1 192.0.2.2

	sleep 1
	jexec alcatraz killall tcpdump

	cat tcpdump.log

	# Make sure we spotted the ridentifier
	atf_check -s exit:0 -o ignore \
	    grep 'rule 2/0.*ridentifier 4321' tcpdump.log
}

basic_cleanup()
{
	pft_cleanup
	rm -f inetd-alcatraz.pid
	rm -f tcpdump.log
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
}
