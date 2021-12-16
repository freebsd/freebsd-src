# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
		"pass keep state"
	jexec two pfctl -e
	pft_set_rules two \
		"set skip on ${epair_sync}b" \
		"pass keep state"

	ifconfig ${epair_one}b 198.51.100.254/24 up

	ping -c 1 -S 198.51.100.254 198.51.100.1

	# Give pfsync time to do its thing
	sleep 2

	if ! jexec two pfctl -s states | grep icmp | grep 198.51.100.1 | \
	    grep 198.51.100.2 ; then
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

	atf_check -s exit:1 env PYTHONPATH=${common_dir} \
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

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "basic_defer"
	atf_add_test_case "defer"
	atf_add_test_case "bulk"
}
