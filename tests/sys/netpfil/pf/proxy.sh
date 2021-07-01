# $FreeBSD$
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

common_dir=$(atf_get_srcdir)/../common

atf_test_case "ftp" "cleanup"
ftp_head()
{
	atf_set descr 'Test ftp-proxy'
	atf_set require.user root
	atf_set require.progs twistd
}

ftp_body()
{
	pft_init

	epair_client=$(vnet_mkepair)
	epair_link=$(vnet_mkepair)

	ifconfig ${epair_client}a 192.0.2.2/24 up
	route add -net 198.51.100.0/24 192.0.2.1

	vnet_mkjail fwd ${epair_client}b ${epair_link}a
	jexec fwd ifconfig ${epair_client}b 192.0.2.1/24 up
	jexec fwd ifconfig ${epair_link}a 198.51.100.1/24 up
	jexec fwd ifconfig lo0 127.0.0.1/8 up
	jexec fwd sysctl net.inet.ip.forwarding=1

	vnet_mkjail srv ${epair_link}b
	jexec srv ifconfig ${epair_link}b 198.51.100.2/24 up
	jexec srv route add default 198.51.100.1

	# Start FTP server in srv
	jexec srv twistd ftp -r `pwd` -p 21

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 198.51.100.2

	jexec fwd pfctl -e
	pft_set_rules fwd \
		"nat on ${epair_link}a inet from 192.0.2.0/24 to any -> (${epair_link}a)" \
		"nat-anchor \"ftp-proxy/*\"" \
		"rdr-anchor \"ftp-proxy/*\"" \
		"rdr pass on ${epair_client}b proto tcp from 192.0.2.0/24 to any port 21 -> 127.0.0.1 port 8021" \
		"anchor \"ftp-proxy/*\"" \
		"pass out proto tcp from 127.0.0.1 to any port 21"
	jexec fwd /usr/sbin/ftp-proxy

	# Create a dummy file to download
	echo 'foo' > remote.txt
	echo 'get remote.txt local.txt' | ftp -a 198.51.100.2

	# Compare the downloaded file to the original
	if ! diff -q local.txt remote.txt;
	then
		atf_fail 'Failed to retrieve file'
	fi
}

ftp_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "ftp"
}
