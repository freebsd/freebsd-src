#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Ahsan Barkati
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
#
#

. $(atf_get_srcdir)/utils.subr
. $(atf_get_srcdir)/runner.subr

v4_head()
{
	atf_set require.user root
}

v4_body()
{   
	firewall=$1
	firewall_init $firewall
	
	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up
	vnet_mkjail iron ${epair}b
	jexec iron ifconfig ${epair}b 192.0.2.2/24 up
	
	# Block All
	firewall_config "iron" ${firewall} \
		"pf" \
			"block in" \
		"ipfw" \
			"ipfw -q add 100 deny all from any to any" \
		"ipf" \
			"block in all"

	atf_check -s exit:2 -o ignore ping -c 1 -t 1 192.0.2.2
	
	# Pass All
	firewall_config "iron" ${firewall} \
		"pf" \
			"pass in" \
		"ipfw" \
			"ipfw -q add 100 allow all from any to any" \
		"ipf" \
			"pass in all"

	atf_check -s exit:0 -o ignore ping -c 1 -t 1 192.0.2.2
}

v4_cleanup()
{
	firewall=$1
	firewall_cleanup $firewall
}

v6_head()
{
	atf_set require.user root
}

v6_body()
{   
	firewall=$1
	firewall_init $firewall
	
	epair=$(vnet_mkepair)
	ifconfig ${epair}a inet6 fd7a:803f:cc4b::1/64 up no_dad

	vnet_mkjail iron ${epair}b
	jexec iron ifconfig ${epair}b inet6 fd7a:803f:cc4b::2/64 up no_dad
	
	# Block All
	firewall_config "iron" ${firewall} \
		"pf" \
			"block in" \
		"ipfw" \
			"ipfw -q add 100 deny all from any to any" \
		"ipf" \
			"block in all"

	atf_check -s exit:2 -o ignore ping -6 -c 1 -W 1 fd7a:803f:cc4b::2
	
	# Pass All
	firewall_config "iron" ${firewall} \
		"pf" \
			"pass in" \
		"ipfw" \
			"ipfw -q add 100 allow all from any to any" \
		"ipf" \
			"pass in all"

	atf_check -s exit:0 -o ignore ping -6 -c 1 -W 1 fd7a:803f:cc4b::2
}

v6_cleanup()
{
	firewall=$1
	firewall_cleanup $firewall
}

setup_tests "v4" \
				"pf" \
				"ipfw" \
				"ipf" \
			"v6" \
				"pf" \
				"ipfw" \
				"ipf"
