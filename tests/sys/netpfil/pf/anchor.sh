#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "pr183198" "cleanup"
pr183198_head()
{
	atf_set descr 'Test tables referenced by rules in anchors'
	atf_set require.user root
}

pr183198_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz pfctl -e

	# Forward with pf enabled
	pft_set_rules alcatraz  \
		"table <test> { 10.0.0.1, 10.0.0.2, 10.0.0.3 }" \
		"block in" \
		"anchor \"epair\" on ${epair}b { \n\
			pass in from <test> \n\
		}"

	atf_check -s exit:0 -o ignore jexec alcatraz pfctl -sr -a '*'
	atf_check -s exit:0 -o ignore jexec alcatraz pfctl -t test -T show
}

pr183198_cleanup()
{
	pft_cleanup
}

atf_test_case "nested_anchor" "cleanup"
nested_anchor_head()
{
	atf_set descr 'Test setting and retrieving nested anchors'
	atf_set require.user root
}

nested_anchor_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	pft_set_rules alcatraz \
		"anchor \"foo\" { \n\
			anchor \"bar\" { \n\
				pass on ${epair}a \n\
			} \n\
		}"

	atf_check -s exit:0 -o inline:"anchor \"foo\" all {
  anchor \"bar\" all {
    pass on ${epair}a all flags S/SA keep state
  }
}
" jexec alcatraz pfctl -sr -a "*"
}

nested_anchor_cleanup()
{
	pft_cleanup
}

atf_test_case "wildcard" "cleanup"
wildcard_head()
{
	atf_set descr 'Test wildcard anchors for functionality'
	atf_set require.user root
}

wildcard_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
		"block" \
		"anchor \"foo/*\""

	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.1

	echo "pass" | jexec alcatraz pfctl -g -f - -a "foo/bar"

	jexec alcatraz pfctl -sr -a "*"
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
}

wildcard_cleanup()
{
	pft_cleanup
}

atf_test_case "nested_label" "cleanup"
nested_label_head()
{
	atf_set descr "Test recursive listing of labels"
	atf_set require.user root
}

nested_label_body()
{
	pft_init

	vnet_mkjail alcatraz

	pft_set_rules alcatraz \
		"anchor \"foo\" { \n\
			pass in quick proto icmp label \"passicmp\"\n\
			anchor \"bar\" { \n\
				pass in proto tcp label \"passtcp\"\n\
			} \n\
		}" \
		"pass quick from any to any label \"anytoany\""

	atf_check -s exit:0 \
	    -o inline:"passicmp 0 0 0 0 0 0 0 0
passtcp 0 0 0 0 0 0 0 0
anytoany 0 0 0 0 0 0 0 0
" jexec alcatraz pfctl -sl -a*
}

nested_label_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "pr183198"
	atf_add_test_case "nested_anchor"
	atf_add_test_case "wildcard"
	atf_add_test_case "nested_label"
}
