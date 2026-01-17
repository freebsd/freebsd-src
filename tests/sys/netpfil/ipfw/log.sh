# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Gleb Smirnoff <glebius@FreeBSD.org>
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

atf_test_case "bpf" "cleanup"
bpf_head()
{
	atf_set descr 'Creates several rules with log and probes bpf taps'
	atf_set require.user root
}

bpf_body()
{
	firewall_init "ipfw"

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b
	ifconfig ${epair}a 192.0.2.0/31 up
	jexec alcatraz ifconfig ${epair}b 192.0.2.1/31 up

	# Create a bunch of statically and auto numbered logging rules
	rules="100 200 201"
	for r in ${rules}; do
		jexec alcatraz \
		    ipfw add ${r} count log udp from any to any 10${r}
	done
	auto=$(jexec alcatraz ipfw add count log udp from any to any 10666 \
	    | awk '{print $1}' | sed -Ee 's/^0+//')

	pids=""
	for r in ${rules} ${auto}; do
		jexec alcatraz tcpdump --immediate-mode -i ipfw${r} \
		    -w ${PWD}/${r}.pcap -c 1 &
		pids="${pids} $!"
	done

	# wait for tcpdumps to attach, include netstat(1) header in ${count}
	count=$(( $(echo ${rules} ${auto} | wc -w) + 1))
	while [ $(jexec alcatraz netstat -B | wc -l) -ne ${count} ]; do
		sleep 0.01;
	done

	for p in ${rules} 666; do
		echo foo | nc -u 192.0.2.1 10${p} -w 0
	done

	for p in ${pids}; do
		atf_check -s exit:0 sh -c "wait $pid; exit $?"
	done

	# statically numbered taps
	for p in ${rules}; do
		atf_check -o match:"192.0.2.0.[0-9]+ > 192.0.2.1.10${p}: UDP" \
		    -e match:"reading from file [a-zA-Z0-9/.]+${p}.pcap" \
		    tcpdump -nr ${PWD}/${p}.pcap
	done

	# autonumbered tap with 10666 port
	atf_check -o match:"192.0.2.0.[0-9]+ > 192.0.2.1.10666: UDP" \
	    -e match:"reading from file [a-zA-Z0-9/.]+${auto}.pcap" \
	    tcpdump -nr ${PWD}/${auto}.pcap
}

bpf_cleanup()
{
	firewall_cleanup $1
}

atf_init_test_cases()
{
	atf_add_test_case "bpf"
}
