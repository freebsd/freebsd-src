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

# Set up two jails, mjail1 and mjail2, connected with two interface pairs
multicast_vnet_init()
{

	vnet_init
	epair1=$(vnet_mkepair)
	epair2=$(vnet_mkepair)
	vnet_mkjail mjail1 ${epair1}a ${epair2}a
	jexec mjail1 ifconfig ${epair1}a up
	jexec mjail1 ifconfig ${epair1}a 192.0.2.1/24
	jexec mjail1 ifconfig ${epair2}a up
	jexec mjail1 ifconfig ${epair2}a 192.0.3.1/24
	vnet_mkjail mjail2 ${epair1}b ${epair2}b
	jexec mjail2 ifconfig ${epair1}b up
	jexec mjail2 ifconfig ${epair1}b 192.0.2.2/24
	jexec mjail2 ifconfig ${epair2}b up
	jexec mjail2 ifconfig ${epair2}b 192.0.3.2/24
}

multicast_join()
{
	jexec mjail2 $(atf_get_srcdir)/multicast-receive \
	    $1 233.252.0.1 6676 $2 > out & pid=$!
	while ! jexec mjail2 ifmcstat | grep -q 233\.252\.0\.1; do
		sleep 0.01
	done
}

atf_test_case "IP_ADD_MEMBERSHIP_ip_mreq" "cleanup"
IP_ADD_MEMBERSHIP_ip_mreq_head()
{
	atf_set descr 'IP_ADD_MEMBERSHIP / IP_MULTICAST_IF with ip_mreq'
	atf_set require.user root
}
IP_ADD_MEMBERSHIP_ip_mreq_body()
{
	multicast_vnet_init

	# join group on interface with IP address 192.0.2.2
	multicast_join ip_mreq 192.0.2.2
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 192.0.2.1 hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.2.1:6676 hello\n" cat out

	# join group on interface with IP address 192.0.3.2
	multicast_join ip_mreq 192.0.3.2
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 192.0.3.1 hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.3.1:6676 hello\n" cat out

	# join group on the first multicast capable interface (epair1a)
	multicast_join ip_mreq 0.0.0.0
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 192.0.2.1 hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.2.1:6676 hello\n" cat out

	# Set up the receiving jail so that first multicast capable interface
	# is epair1a and default route points into epair2a.  This will allow us
	# to exercise both branches of inp_lookup_mcast_ifp().
	jexec mjail2 route add default 192.0.3.254

	# join group on the interface determined by the route lookup
	multicast_join ip_mreq 0.0.0.0
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 192.0.3.1 hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.3.1:6676 hello\n" cat out
}
IP_ADD_MEMBERSHIP_ip_mreq_cleanup()
{
	rm out
	vnet_cleanup
}

atf_test_case "IP_ADD_MEMBERSHIP_ip_mreqn" "cleanup"
IP_ADD_MEMBERSHIP_ip_mreqn_head()
{
	atf_set descr 'IP_ADD_MEMBERSHIP / IP_MULTICAST_IF with ip_mreqn'
	atf_set require.user root
}
IP_ADD_MEMBERSHIP_ip_mreqn_body()
{
	multicast_vnet_init

	# join group on interface epair2
	multicast_join ip_mreqn ${epair1}b
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 ${epair1}a hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.2.1:6676 hello\n" cat out

	# join group on interface epair2
	multicast_join ip_mreqn ${epair2}b
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 ${epair2}a hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.3.1:6676 hello\n" cat out

	# try to join group on the interface determined by the route lookup
	atf_check -s exit:71 -e inline:"multicast-receive: setsockopt: Can't assign requested address\n" \
	    jexec mjail2 $(atf_get_srcdir)/multicast-receive \
	    ip_mreqn 233.252.0.1 6676 0
	# add route and try again
	jexec mjail2 route add default 192.0.3.254
        multicast_join ip_mreqn 0
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 192.0.3.1 hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.3.1:6676 hello\n" cat out
}
IP_ADD_MEMBERSHIP_ip_mreqn_cleanup()
{
	rm out
	vnet_cleanup
}

atf_test_case "MCAST_JOIN_GROUP" "cleanup"
MCAST_JOIN_GROUP_head()
{
	atf_set descr 'MCAST_JOIN_GROUP'
	atf_set require.user root
}
MCAST_JOIN_GROUP_body()
{
	multicast_vnet_init

	# join group on interface epair1
	multicast_join group_req ${epair1}b
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 ${epair1}a hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.2.1:6676 hello\n" cat out

	# join group on interface epair2
	multicast_join group_req ${epair2}b
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 ${epair2}a hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.3.1:6676 hello\n" cat out

	# try to join group on the interface determined by the route lookup
	atf_check -s exit:71 -e inline:"multicast-receive: setsockopt: Can't assign requested address\n" \
	    jexec mjail2 $(atf_get_srcdir)/multicast-receive \
	    group_req 233.252.0.1 6676 0
	# add route and try again
	jexec mjail2 route add default 192.0.3.254
        multicast_join group_req 0
	atf_check -s exit:0 -o empty \
	    jexec mjail1 $(atf_get_srcdir)/multicast-send \
	    0.0.0.0 6676 233.252.0.1 6676 192.0.3.1 hello
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o inline:"192.0.3.1:6676 hello\n" cat out
}
MCAST_JOIN_GROUP_cleanup()
{
	rm out
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "IP_ADD_MEMBERSHIP_ip_mreq"
	atf_add_test_case "IP_ADD_MEMBERSHIP_ip_mreqn"
	atf_add_test_case "MCAST_JOIN_GROUP"
}
