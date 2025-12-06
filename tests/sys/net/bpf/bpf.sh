##
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Rubicon Communications, LLC ("Netgate")
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

. $(atf_get_srcdir)/../../common/vnet.subr

atf_test_case "multi_read" "cleanup"
multi_read_head()
{
	atf_set descr 'Test multiple readers on /dev/bpf'
	atf_set require.user root
}
multi_read_body()
{
	vnet_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet 192.0.2.2/24 up

	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.2

	# Start a multi-thread (or multi-process) read on bpf
	$(atf_get_srcdir)/bpf_multi_read ${epair}a &

	# Generate traffic
	ping -f 192.0.2.2 >/dev/null 2>&1 &

	# Now let this run for 10 seconds
	sleep 10
}
multi_read_cleanup()
{
	vnet_cleanup
}

atf_test_case "inject" "cleanup"
inject_head()
{
	atf_set descr 'Catch packets, re-inject and check'
	atf_set require.user root
}
inject_body()
{
	vnet_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet 192.0.2.1/24 up
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b inet 192.0.2.2/24 up

	in=$(pwd)/$(mktemp in.pcap.XXXXXXXXXX)
	in2=$(pwd)/$(mktemp in2.pcap.XXXXXXXXXX)
	out=$(pwd)/$(mktemp out.pcap.XXXXXXXXXX)

	# write dump on jail side, with "in" direction
	jexec alcatraz $(atf_get_srcdir)/pcap-test \
	    capture epair0b $in 3 in > out & pid=$!
	while ! jexec alcatraz netstat -B | grep -q epair0b.*pcap-test; do
		sleep 0.01;
	done
	atf_check -s exit:0 -o ignore ping -c 3 -i 0.1 192.0.2.2
	atf_check -s exit:0 sh -c "wait $pid; exit $?"
	atf_check -s exit:0 -o empty cat out

	# inject dump on host side, recording on both sides
	jexec alcatraz $(atf_get_srcdir)/pcap-test \
	    capture epair0b $in2 3 in > jout & jpid=$!
	while ! jexec alcatraz netstat -B | grep -q epair0b.*pcap-test; do
		sleep 0.01;
	done
	$(atf_get_srcdir)/pcap-test \
	    capture epair0a $out 3 out > hout & hpid=$!
	while ! netstat -B | grep -q epair0a.*pcap-test; do
		sleep 0.01;
	done
	atf_check -s exit:0 -o empty -e empty $(atf_get_srcdir)/pcap-test \
	    inject epair0a $in 3
	atf_check -s exit:0 sh -c "wait $jpid; exit $?"
	atf_check -s exit:0 -o empty cat jout
	atf_check -s exit:0 sh -c "wait $hpid; exit $?"
	atf_check -s exit:0 -o empty cat hout

	# all 3 dumps should be equal
	atf_check -s exit:0 -o empty -e empty $(atf_get_srcdir)/pcap-test \
	    compare $in $out
	atf_check -s exit:0 -o empty -e empty $(atf_get_srcdir)/pcap-test \
	    compare $in $in2
}
inject_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "multi_read"
	atf_add_test_case "inject"
}
