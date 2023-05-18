# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Orange Business Services
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

common_dir=$(atf_get_srcdir)/../common

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic pfsync test'
	atf_set require.user root
}

basic_body()
{
	common_body
}

common_body()
{
	defer=$1
	pfsynct_init

	epair_sync=$(vnet_mkepair)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail one ${epair_one}a ${epair_sync}a
	vnet_mkjail two ${epair_two}a ${epair_sync}b

	# pfsync interface
	jexec one ifconfig ${epair_sync}a 192.0.2.1/24 up
	jexec one ifconfig ${epair_one}a 198.51.100.1/24 up
	jexec one ifconfig pfsync0 \
		syncdev ${epair_sync}a \
		maxupd 1 \
		$defer \
		up
	jexec two ifconfig ${epair_two}a 198.51.100.2/24 up
	jexec two ifconfig ${epair_sync}b 192.0.2.2/24 up
	jexec two ifconfig pfsync0 \
		syncdev ${epair_sync}b \
		maxupd 1 \
		$defer \
		up

	# Enable pf!
	jexec one pfctl -e
	pft_set_rules one \
		"set skip on ${epair_sync}a" \
		"pass out keep state"
	jexec two pfctl -e
	pft_set_rules two \
		"set skip on ${epair_sync}b" \
		"pass out keep state"

	ifconfig ${epair_one}b 198.51.100.254/24 up

	ping -c 1 -S 198.51.100.254 198.51.100.1

	# Give pfsync time to do its thing
	sleep 2

	if ! jexec two pfctl -s states | grep icmp | grep 198.51.100.1 | \
	    grep 198.51.100.254 ; then
		atf_fail "state not found on synced host"
	fi
}

basic_cleanup()
{
	pfsynct_cleanup
}

atf_test_case "basic_defer" "cleanup"
basic_defer_head()
{
	atf_set descr 'Basic defer mode pfsync test'
	atf_set require.user root
}

basic_defer_body()
{
	common_body defer
}

basic_defer_cleanup()
{
	pfsynct_cleanup
}

atf_test_case "defer" "cleanup"
defer_head()
{
	atf_set descr 'Defer mode pfsync test'
	atf_set require.user root
}

defer_body()
{
	pfsynct_init

	if [ "$(atf_config_get ci false)" = "true" ]; then
		atf_skip "Skip know failing test (likely related to https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=260460)"
	fi

	epair_sync=$(vnet_mkepair)
	epair_in=$(vnet_mkepair)
	epair_out=$(vnet_mkepair)

	vnet_mkjail alcatraz ${epair_sync}a ${epair_in}a ${epair_out}a

	jexec alcatraz ifconfig ${epair_sync}a 192.0.2.1/24 up
	jexec alcatraz ifconfig ${epair_out}a 198.51.100.1/24 up
	jexec alcatraz ifconfig ${epair_in}a 203.0.113.1/24 up
	jexec alcatraz arp -s 203.0.113.2 00:01:02:03:04:05
	jexec alcatraz sysctl net.inet.ip.forwarding=1

	jexec alcatraz ifconfig pfsync0 \
		syncdev ${epair_sync}a \
		maxupd 1 \
		defer \
		up

	ifconfig ${epair_sync}b 192.0.2.2/24 up
	ifconfig ${epair_out}b 198.51.100.2/24 up
	ifconfig ${epair_in}b up
	route add -net 203.0.113.0/24 198.51.100.1

	# Enable pf
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"set skip on ${epair_sync}a" \
		"pass keep state"

	atf_check -s exit:0 env PYTHONPATH=${common_dir} \
		$(atf_get_srcdir)/pfsync_defer.py \
		--syncdev ${epair_sync}b \
		--indev ${epair_in}b \
		--outdev ${epair_out}b

	# Now disable defer mode and expect failure.
	jexec alcatraz ifconfig pfsync0 -defer

	# Flush state
	pft_set_rules alcatraz \
		"set skip on ${epair_sync}a" \
		"pass keep state"

	atf_check -s exit:3 env PYTHONPATH=${common_dir} \
		$(atf_get_srcdir)/pfsync_defer.py \
		--syncdev ${epair_sync}b \
		--indev ${epair_in}b \
		--outdev ${epair_out}b
}

defer_cleanup()
{
	pfsynct_cleanup
}

atf_test_case "bulk" "cleanup"
bulk_head()
{
	atf_set descr 'Test bulk updates'
	atf_set require.user root
}

bulk_body()
{
	pfsynct_init

	epair_sync=$(vnet_mkepair)
	epair_one=$(vnet_mkepair)
	epair_two=$(vnet_mkepair)

	vnet_mkjail one ${epair_one}a ${epair_sync}a
	vnet_mkjail two ${epair_two}a ${epair_sync}b

	# pfsync interface
	jexec one ifconfig ${epair_sync}a 192.0.2.1/24 up
	jexec one ifconfig ${epair_one}a 198.51.100.1/24 up
	jexec one ifconfig pfsync0 \
		syncdev ${epair_sync}a \
		maxupd 1\
		up
	jexec two ifconfig ${epair_two}a 198.51.100.2/24 up
	jexec two ifconfig ${epair_sync}b 192.0.2.2/24 up

	# Enable pf
	jexec one pfctl -e
	pft_set_rules one \
		"set skip on ${epair_sync}a" \
		"pass keep state"
	jexec two pfctl -e
	pft_set_rules two \
		"set skip on ${epair_sync}b" \
		"pass keep state"

	ifconfig ${epair_one}b 198.51.100.254/24 up

	# Create state prior to setting up pfsync
	ping -c 1 -S 198.51.100.254 198.51.100.1

	# Wait before setting up pfsync on two, so we don't accidentally catch
	# the update anyway.
	sleep 1

	# Now set up pfsync in jail two
	jexec two ifconfig pfsync0 \
		syncdev ${epair_sync}b \
		up

	# Give pfsync time to do its thing
	sleep 2

	jexec two pfctl -s states
	if ! jexec two pfctl -s states | grep icmp | grep 198.51.100.1 | \
	    grep 198.51.100.2 ; then
		atf_fail "state not found on synced host"
	fi
}

bulk_cleanup()
{
	pfsynct_cleanup
}

atf_test_case "pbr" "cleanup"
pbr_head()
{
	atf_set descr 'route_to and reply_to directives test'
	atf_set require.user root
	atf_set timeout '600'
}

pbr_body()
{
	pbr_common_body
}

pbr_cleanup()
{
	pbr_common_cleanup
}

atf_test_case "pfsync_pbr" "cleanup"
pfsync_pbr_head()
{
	atf_set descr 'route_to and reply_to directives pfsync test'
	atf_set require.user root
	atf_set timeout '600'
}

pfsync_pbr_body()
{
	pbr_common_body backup_promotion
}

pfsync_pbr_cleanup()
{
	pbr_common_cleanup
}

pbr_common_body()
{
	# + builds bellow topology and initiate a single ping session
	#   from client to server.
	# + gw* forward traffic through pbr not fib lookups.
	# + if backup_promotion arg is given, a carp failover event occurs
	#   during the ping session on both gateways.
	#                   ┌──────┐
	#                   │client│
	#                   └───┬──┘
	#                       │
	#                   ┌───┴───┐
	#                   │bridge0│
	#                   └┬─────┬┘
	#                    │     │
	#   ┌────────────────┴─┐ ┌─┴────────────────┐
	#   │gw_route_to_master├─┤gw_route_to_backup│
	#   └────────────────┬─┘ └─┬────────────────┘
	#                    │     │
	#                   ┌┴─────┴┐
	#                   │bridge1│
	#                   └┬─────┬┘
	#                    │     │
	#   ┌────────────────┴─┐ ┌─┴────────────────┐
	#   │gw_reply_to_master├─┤gw_reply_to_backup│
	#   └────────────────┬─┘ └─┬────────────────┘
	#                    │     │
	#                   ┌┴─────┴┐
	#                   │bridge2│
	#                   └───┬───┘
	#                       │
	#                   ┌───┴──┐
	#                   │server│
	#                   └──────┘

	if ! kldstat -q -m carp
	then
		atf_skip "This test requires carp"
	fi
	pfsynct_init

	bridge0=$(vnet_mkbridge)
	bridge1=$(vnet_mkbridge)
	bridge2=$(vnet_mkbridge)

	epair_sync_gw_route_to=$(vnet_mkepair)
	epair_sync_gw_reply_to=$(vnet_mkepair)
	epair_client_bridge0=$(vnet_mkepair)

	epair_gw_route_to_master_bridge0=$(vnet_mkepair)
	epair_gw_route_to_backup_bridge0=$(vnet_mkepair)
	epair_gw_route_to_master_bridge1=$(vnet_mkepair)
	epair_gw_route_to_backup_bridge1=$(vnet_mkepair)

	epair_gw_reply_to_master_bridge1=$(vnet_mkepair)
	epair_gw_reply_to_backup_bridge1=$(vnet_mkepair)
	epair_gw_reply_to_master_bridge2=$(vnet_mkepair)
	epair_gw_reply_to_backup_bridge2=$(vnet_mkepair)

	epair_server_bridge2=$(vnet_mkepair)

	ifconfig ${bridge0} up
	ifconfig ${epair_client_bridge0}b up
	ifconfig ${epair_gw_route_to_master_bridge0}b up
	ifconfig ${epair_gw_route_to_backup_bridge0}b up
	ifconfig ${bridge0} \
		addm ${epair_client_bridge0}b \
		addm ${epair_gw_route_to_master_bridge0}b \
		addm ${epair_gw_route_to_backup_bridge0}b

	ifconfig ${bridge1} up
	ifconfig ${epair_gw_route_to_master_bridge1}b up
	ifconfig ${epair_gw_route_to_backup_bridge1}b up
	ifconfig ${epair_gw_reply_to_master_bridge1}b up
	ifconfig ${epair_gw_reply_to_backup_bridge1}b up
	ifconfig ${bridge1} \
		addm ${epair_gw_route_to_master_bridge1}b \
		addm ${epair_gw_route_to_backup_bridge1}b \
		addm ${epair_gw_reply_to_master_bridge1}b \
		addm ${epair_gw_reply_to_backup_bridge1}b

	ifconfig ${bridge2} up
	ifconfig ${epair_gw_reply_to_master_bridge2}b up
	ifconfig ${epair_gw_reply_to_backup_bridge2}b up
	ifconfig ${epair_server_bridge2}b up
	ifconfig ${bridge2} \
		addm ${epair_gw_reply_to_master_bridge2}b \
		addm ${epair_gw_reply_to_backup_bridge2}b \
		addm ${epair_server_bridge2}b

	vnet_mkjail client ${epair_client_bridge0}a
	jexec client hostname client
	vnet_mkjail gw_route_to_master \
		${epair_gw_route_to_master_bridge0}a \
		${epair_gw_route_to_master_bridge1}a \
		${epair_sync_gw_route_to}a
	jexec gw_route_to_master hostname gw_route_to_master
	vnet_mkjail gw_route_to_backup \
		${epair_gw_route_to_backup_bridge0}a \
		${epair_gw_route_to_backup_bridge1}a \
		${epair_sync_gw_route_to}b
	jexec gw_route_to_backup hostname gw_route_to_backup
	vnet_mkjail gw_reply_to_master \
		${epair_gw_reply_to_master_bridge1}a \
		${epair_gw_reply_to_master_bridge2}a \
		${epair_sync_gw_reply_to}a
	jexec gw_reply_to_master hostname gw_reply_to_master
	vnet_mkjail gw_reply_to_backup \
		${epair_gw_reply_to_backup_bridge1}a \
		${epair_gw_reply_to_backup_bridge2}a \
		${epair_sync_gw_reply_to}b
	jexec gw_reply_to_backup hostname gw_reply_to_backup
	vnet_mkjail server ${epair_server_bridge2}a
	jexec server hostname server

	jexec client ifconfig ${epair_client_bridge0}a inet 198.18.0.1/24 up
	jexec client route add 198.18.2.0/24 198.18.0.10

	jexec gw_route_to_master ifconfig ${epair_sync_gw_route_to}a \
		inet 198.19.10.1/24 up
	jexec gw_route_to_master ifconfig ${epair_gw_route_to_master_bridge0}a \
		inet 198.18.0.8/24 up
	jexec gw_route_to_master ifconfig ${epair_gw_route_to_master_bridge0}a \
		alias 198.18.0.10/32 vhid 10 pass 3WjvVVw7 advskew 50
	jexec gw_route_to_master ifconfig ${epair_gw_route_to_master_bridge1}a \
		inet 198.18.1.8/24 up
	jexec gw_route_to_master ifconfig ${epair_gw_route_to_master_bridge1}a \
		alias 198.18.1.10/32 vhid 11 pass 3WjvVVw7 advskew 50
	jexec gw_route_to_master sysctl net.inet.ip.forwarding=1
	jexec gw_route_to_master sysctl net.inet.carp.preempt=1

	vnet_ifrename_jail gw_route_to_master ${epair_sync_gw_route_to}a if_pfsync
	vnet_ifrename_jail gw_route_to_master ${epair_gw_route_to_master_bridge0}a if_br0
	vnet_ifrename_jail gw_route_to_master ${epair_gw_route_to_master_bridge1}a if_br1

	jexec gw_route_to_master ifconfig pfsync0 \
		syncpeer 198.19.10.2 \
		syncdev if_pfsync \
		maxupd 1 \
		up
	pft_set_rules gw_route_to_master \
		"keep_state = 'tag auth_packet keep state'" \
		"set timeout { icmp.first 120, icmp.error 60 }" \
		"block log all" \
		"pass quick on if_pfsync proto pfsync keep state (no-sync)" \
		"pass quick on { if_br0 if_br1 } proto carp keep state (no-sync)" \
		"block drop in quick to 224.0.0.18/32" \
		"pass out quick tagged auth_packet keep state" \
		"pass in quick log on if_br0 route-to (if_br1 198.18.1.20) proto { icmp udp tcp } from 198.18.0.0/24 to 198.18.2.0/24 \$keep_state"
	jexec gw_route_to_master pfctl -e

	jexec gw_route_to_backup ifconfig ${epair_sync_gw_route_to}b \
		inet 198.19.10.2/24 up
	jexec gw_route_to_backup ifconfig ${epair_gw_route_to_backup_bridge0}a \
		inet 198.18.0.9/24 up
	jexec gw_route_to_backup ifconfig ${epair_gw_route_to_backup_bridge0}a \
		alias 198.18.0.10/32 vhid 10 pass 3WjvVVw7 advskew 100
	jexec gw_route_to_backup ifconfig ${epair_gw_route_to_backup_bridge1}a \
		inet 198.18.1.9/24 up
	jexec gw_route_to_backup ifconfig ${epair_gw_route_to_backup_bridge1}a \
		alias 198.18.1.10/32 vhid 11 pass 3WjvVVw7 advskew 100
	jexec gw_route_to_backup sysctl net.inet.ip.forwarding=1
	jexec gw_route_to_backup sysctl net.inet.carp.preempt=1

	vnet_ifrename_jail gw_route_to_backup ${epair_sync_gw_route_to}b if_pfsync
	vnet_ifrename_jail gw_route_to_backup ${epair_gw_route_to_backup_bridge0}a if_br0
	vnet_ifrename_jail gw_route_to_backup ${epair_gw_route_to_backup_bridge1}a if_br1

	jexec gw_route_to_backup ifconfig pfsync0 \
		syncpeer 198.19.10.1 \
		syncdev if_pfsync \
		up
	pft_set_rules gw_route_to_backup \
		"keep_state = 'tag auth_packet keep state'" \
		"set timeout { icmp.first 120, icmp.error 60 }" \
		"block log all" \
		"pass quick on if_pfsync proto pfsync keep state (no-sync)" \
		"pass quick on { if_br0 if_br1 } proto carp keep state (no-sync)" \
		"block drop in quick to 224.0.0.18/32" \
		"pass out quick tagged auth_packet keep state" \
		"pass in quick log on if_br0 route-to (if_br1 198.18.1.20) proto { icmp udp tcp } from 198.18.0.0/24 to 198.18.2.0/24 \$keep_state"
	jexec gw_route_to_backup pfctl -e

	jexec gw_reply_to_master ifconfig ${epair_sync_gw_reply_to}a \
		inet 198.19.20.1/24 up
	jexec gw_reply_to_master ifconfig ${epair_gw_reply_to_master_bridge1}a \
		inet 198.18.1.18/24 up
	jexec gw_reply_to_master ifconfig ${epair_gw_reply_to_master_bridge1}a \
		alias 198.18.1.20/32 vhid 21 pass 3WjvVVw7 advskew 50
	jexec gw_reply_to_master ifconfig ${epair_gw_reply_to_master_bridge2}a \
		inet 198.18.2.18/24 up
	jexec gw_reply_to_master ifconfig ${epair_gw_reply_to_master_bridge2}a \
		alias 198.18.2.20/32 vhid 22 pass 3WjvVVw7 advskew 50
	jexec gw_reply_to_master sysctl net.inet.ip.forwarding=1
	jexec gw_reply_to_master sysctl net.inet.carp.preempt=1

	vnet_ifrename_jail gw_reply_to_master ${epair_sync_gw_reply_to}a if_pfsync
	vnet_ifrename_jail gw_reply_to_master ${epair_gw_reply_to_master_bridge1}a if_br1
	vnet_ifrename_jail gw_reply_to_master ${epair_gw_reply_to_master_bridge2}a if_br2

	jexec gw_reply_to_master ifconfig pfsync0 \
		syncpeer 198.19.20.2 \
		syncdev if_pfsync \
		maxupd 1 \
		up
	pft_set_rules gw_reply_to_master \
		"set timeout { icmp.first 120, icmp.error 60 }" \
		"block log all" \
		"pass quick on if_pfsync proto pfsync keep state (no-sync)" \
		"pass quick on { if_br1 if_br2 } proto carp keep state (no-sync)" \
		"block drop in quick to 224.0.0.18/32" \
		"pass out quick on if_br2 reply-to (if_br1 198.18.1.10) tagged auth_packet_reply_to keep state" \
		"pass in quick log on if_br1 proto { icmp udp tcp } from 198.18.0.0/24 to 198.18.2.0/24 tag auth_packet_reply_to keep state"
	jexec gw_reply_to_master pfctl -e

	jexec gw_reply_to_backup ifconfig ${epair_sync_gw_reply_to}b \
		inet 198.19.20.2/24 up
	jexec gw_reply_to_backup ifconfig ${epair_gw_reply_to_backup_bridge1}a \
		inet 198.18.1.19/24 up
	jexec gw_reply_to_backup ifconfig ${epair_gw_reply_to_backup_bridge1}a \
		alias 198.18.1.20/32 vhid 21 pass 3WjvVVw7 advskew 100
	jexec gw_reply_to_backup ifconfig ${epair_gw_reply_to_backup_bridge2}a \
		inet 198.18.2.19/24 up
	jexec gw_reply_to_backup ifconfig ${epair_gw_reply_to_backup_bridge2}a \
		alias 198.18.2.20/32 vhid 22 pass 3WjvVVw7 advskew 100
	jexec gw_reply_to_backup sysctl net.inet.ip.forwarding=1
	jexec gw_reply_to_backup sysctl net.inet.carp.preempt=1

	vnet_ifrename_jail gw_reply_to_backup ${epair_sync_gw_reply_to}b if_pfsync
	vnet_ifrename_jail gw_reply_to_backup ${epair_gw_reply_to_backup_bridge1}a if_br1
	vnet_ifrename_jail gw_reply_to_backup ${epair_gw_reply_to_backup_bridge2}a if_br2

	jexec gw_reply_to_backup ifconfig pfsync0 \
		syncpeer 198.19.20.1 \
		syncdev if_pfsync \
		up
	pft_set_rules gw_reply_to_backup \
		"set timeout { icmp.first 120, icmp.error 60 }" \
		"block log all" \
		"pass quick on if_pfsync proto pfsync keep state (no-sync)" \
		"pass quick on { if_br1 if_br2 } proto carp keep state (no-sync)" \
		"block drop in quick to 224.0.0.18/32" \
		"pass out quick on if_br2 reply-to (if_br1 198.18.1.10) tagged auth_packet_reply_to keep state" \
		"pass in quick log on if_br1 proto { icmp udp tcp } from 198.18.0.0/24 to 198.18.2.0/24 tag auth_packet_reply_to keep state"
	jexec gw_reply_to_backup pfctl -e

	jexec server ifconfig ${epair_server_bridge2}a inet 198.18.2.1/24 up
	jexec server route add 198.18.0.0/24 198.18.2.20

	# Waiting for platform to settle
	while ! jexec gw_route_to_backup ifconfig | grep 'carp: BACKUP'
	do
		sleep 1
	done
	while ! jexec gw_reply_to_backup ifconfig | grep 'carp: BACKUP'
	do
		sleep 1
	done
	while ! jexec client ping -c 10 198.18.2.1 | grep ', 0.0% packet loss'
	do
		sleep 1
	done

	# Checking cluster members pf.conf checksums match
	gw_route_to_master_checksum=$(jexec gw_route_to_master pfctl -si -v | grep 'Checksum:' | cut -d ' ' -f 2)
	gw_route_to_backup_checksum=$(jexec gw_route_to_backup pfctl -si -v | grep 'Checksum:' | cut -d ' ' -f 2)
	gw_reply_to_master_checksum=$(jexec gw_reply_to_master pfctl -si -v | grep 'Checksum:' | cut -d ' ' -f 2)
	gw_reply_to_backup_checksum=$(jexec gw_reply_to_backup pfctl -si -v | grep 'Checksum:' | cut -d ' ' -f 2)
	if [ "$gw_route_to_master_checksum" != "$gw_route_to_backup_checksum" ]
	then
		atf_fail "gw_route_to cluster members pf.conf do not match each others"
	fi
	if [ "$gw_reply_to_master_checksum" != "$gw_reply_to_backup_checksum" ]
	then
		atf_fail "gw_reply_to cluster members pf.conf do not match each others"
	fi

	# Creating state entries
	(jexec client ping -c 10 198.18.2.1 >ping.stdout) &

	if [ "$1" = "backup_promotion" ]
	then
		sleep 1
		jexec gw_route_to_backup ifconfig if_br0 vhid 10 advskew 0
		jexec gw_route_to_backup ifconfig if_br1 vhid 11 advskew 0
		jexec gw_reply_to_backup ifconfig if_br1 vhid 21 advskew 0
		jexec gw_reply_to_backup ifconfig if_br2 vhid 22 advskew 0
	fi
	while ! grep -q -e 'packet loss' ping.stdout
	do
		sleep 1
	done

	atf_check -s exit:0 -e ignore -o ignore grep ', 0.0% packet loss' ping.stdout
}

pbr_common_cleanup()
{
	pft_cleanup
}

atf_test_case "ipsec" "cleanup"
ipsec_head()
{
	atf_set descr 'Transport pfsync over IPSec'
	atf_set require.user root
}

ipsec_body()
{
	if ! sysctl -q kern.features.ipsec >/dev/null ; then
		atf_skip "This test requires ipsec"
	fi

	# Run the common test, to set up pfsync
	common_body

	# But we want unicast pfsync
	jexec one ifconfig pfsync0 syncpeer 192.0.2.2
	jexec two ifconfig pfsync0 syncpeer 192.0.2.1

	# Flush existing states
	jexec one pfctl -Fs
	jexec two pfctl -Fs

	# Now define an ipsec policy to run over the epair_sync interfaces
	echo "flush;
	spdflush;
	spdadd 192.0.2.1/32 192.0.2.2/32 any -P out ipsec esp/transport//require;
	spdadd 192.0.2.2/32 192.0.2.1/32 any -P in ipsec esp/transport//require;
	add 192.0.2.1 192.0.2.2 esp 0x1000 -E aes-gcm-16 \"12345678901234567890\";
	add 192.0.2.2 192.0.2.1 esp 0x1001 -E aes-gcm-16 \"12345678901234567890\";" \
	    | jexec one setkey -c

	echo "flush;
	spdflush;
	spdadd 192.0.2.2/32 192.0.2.1/32 any -P out ipsec esp/transport//require;
	spdadd 192.0.2.1/32 192.0.2.2/32 any -P in ipsec esp/transport//require;
	add 192.0.2.1 192.0.2.2 esp 0x1000 -E aes-gcm-16 \"12345678901234567891\";
	add 192.0.2.2 192.0.2.1 esp 0x1001 -E aes-gcm-16 \"12345678901234567891\";" \
	    | jexec two setkey -c

	# We've set incompatible keys, so pfsync will be broken.
	ping -c 1 -S 198.51.100.254 198.51.100.1

	# Give pfsync time to do its thing
	sleep 2

	if jexec two pfctl -s states | grep icmp | grep 198.51.100.1 | \
	    grep 198.51.100.2 ; then
		atf_fail "state synced although IPSec should have prevented it"
	fi

	# Flush existing states
	jexec one pfctl -Fs
	jexec two pfctl -Fs

	# Fix the IPSec key to match
	echo "flush;
	spdflush;
	spdadd 192.0.2.2/32 192.0.2.1/32 any -P out ipsec esp/transport//require;
	spdadd 192.0.2.1/32 192.0.2.2/32 any -P in ipsec esp/transport//require;
	add 192.0.2.1 192.0.2.2 esp 0x1000 -E aes-gcm-16 \"12345678901234567890\";
	add 192.0.2.2 192.0.2.1 esp 0x1001 -E aes-gcm-16 \"12345678901234567890\";" \
	    | jexec two setkey -c

	ping -c 1 -S 198.51.100.254 198.51.100.1

	# Give pfsync time to do its thing
	sleep 2

	if ! jexec two pfctl -s states | grep icmp | grep 198.51.100.1 | \
	    grep 198.51.100.2 ; then
		atf_fail "state not found on synced host"
	fi
}

ipsec_cleanup()
{
	pft_cleanup
}

atf_test_case "timeout" "cleanup"
timeout_head()
{
	atf_set descr 'Trigger pfsync_timeout()'
	atf_set require.user root
}

timeout_body()
{
	pft_init

	vnet_mkjail one

	jexec one ifconfig lo0 127.0.0.1/8 up
	jexec one ifconfig lo0 inet6 ::1/128 up

	pft_set_rules one \
		"pass all"
	jexec one pfctl -e
	jexec one ifconfig pfsync0 defer up

	jexec one ping -c 1 ::1
	jexec one ping -c 1 127.0.0.1

	# Give pfsync_timeout() time to fire (a callout on a 1 second delay)
	sleep 2
}

timeout_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "basic_defer"
	atf_add_test_case "defer"
	atf_add_test_case "bulk"
	atf_add_test_case "pbr"
	atf_add_test_case "pfsync_pbr"
	atf_add_test_case "ipsec"
	atf_add_test_case "timeout"
}
