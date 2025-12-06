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

atf_test_case "pr279225" "cleanup"
pr279225_head()
{
	atf_set descr "Test that we can retrieve longer anchor names, PR 279225"
	atf_set require.user root
}

pr279225_body()
{
	pft_init

	vnet_mkjail alcatraz

	pft_set_rules alcatraz \
		"nat-anchor \"appjail-nat/jail/*\" all" \
		"rdr-anchor \"appjail-rdr/*\" all" \
		"anchor \"appjail/jail/*\" all"

	atf_check -s exit:0 -o match:"nat-anchor \"appjail-nat/jail/\*\" all \{" \
		jexec alcatraz pfctl -sn -a "*"
	atf_check -s exit:0 -o match:"rdr-anchor \"appjail-rdr/\*\" all \{" \
		jexec alcatraz pfctl -sn -a "*"
	atf_check -s exit:0 -o match:"anchor \"appjail/jail/\*\" all \{" \
		jexec alcatraz pfctl -sr -a "*"
}

pr279225_cleanup()
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

atf_test_case "deeply_nested" "cleanup"
deeply_nested_head()
{
	atf_set descr 'Test setting and retrieving deeply nested anchors'
	atf_set require.user root
}

deeply_nested_body()
{
	pft_init

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	pft_set_rules alcatraz \
		"anchor \"foo\" { \n\
			anchor \"bar\" { \n\
				anchor \"foobar\" { \n\
					pass on ${epair}a \n\
				} \n\
				anchor \"quux\" { \n\
					pass on ${epair}a \n\
				} \n\
			} \n\
			anchor \"baz\" { \n\
				pass on ${epair}a \n\
			} \n\
			anchor \"qux\" { \n\
				pass on ${epair}a \n\
			} \n\
		}"

	atf_check -s exit:0 -o \
	    inline:"  foo\n  foo/bar\n  foo/bar/foobar\n  foo/bar/quux\n  foo/baz\n  foo/qux\n" \
	    jexec alcatraz pfctl -sA

	atf_check -s exit:0 -o inline:"  foo/bar/foobar\n  foo/bar/quux\n" \
	    jexec alcatraz pfctl -a foo/bar -sA
}

deeply_nested_cleanup()
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

atf_test_case "quick" "cleanup"
quick_head()
{
	atf_set descr "Test handling of quick on anchors"
	atf_set require.user root
}

quick_body()
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
	    "anchor quick {\n\
	        pass\n\
	    }" \
	    "block"

	# We can still ping because the anchor is 'quick'
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
	jexec alcatraz pfctl -sr -v
	jexec alcatraz pfctl -ss -v
}

quick_cleanup()
{
	pft_cleanup
}

atf_test_case "quick_nested" "cleanup"
quick_nested_head()
{
	atf_set descr 'Verify that a nested anchor does not clear quick'
	atf_set require.user root
}

quick_nested_body()
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
	    "anchor quick {\n\
	        pass\n\
	        anchor {\n\
	            block proto tcp\n\
	        }\n\
	    }" \
	    "block"
	ping -c 1 192.0.2.1

	jexec alcatraz pfctl -sr -v
	jexec alcatraz pfctl -ss -v

	# We can still ping because the anchor is 'quick'
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
	jexec alcatraz pfctl -sr -v
	jexec alcatraz pfctl -ss -v
}

quick_nested_cleanup()
{
	pft_cleanup
}

atf_test_case "counter" "cleanup"
counter_head()
{
	atf_set descr 'Test counters on anchors'
	atf_set require.user root
}

counter_body()
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
	    "anchor \"foo\" {\n\
	        pass\n\
	    }"

	# Generate traffic
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
	atf_check -s exit:0 -e ignore \
	    -o match:'[ Evaluations: 1         Packets: 2         Bytes: 168         States: 1     ]' \
	    jexec alcatraz pfctl -sr -vv
}

counter_cleanup()
{
	pft_cleanup
}

atf_test_case "nat" "cleanup"
nat_head()
{
	atf_set descr 'Test nested nat anchors'
	atf_set require.user root
}

nat_body()
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
	    "nat-anchor \"foo/*\"" \
	    "pass"

	echo "nat log on ${epair}a inet from 192.0.2.0/24 to any port = 53 -> 192.0.2.1" \
	    | jexec alcatraz pfctl -a "foo/bar" -g -f -
	echo "rdr on ${epair}a proto tcp to port echo -> 127.0.0.1 port echo" \
	    | jexec alcatraz pfctl -a "foo/baz" -g -f -

	jexec alcatraz pfctl -sn -a "*"
	jexec alcatraz pfctl -sn -a "foo/bar"
	jexec alcatraz pfctl -sn -a "foo/baz"

	atf_check -s exit:0 -o match:"nat log on ${epair}a inet from 192.0.2.0/24 to any port = domain -> 192.0.2.1" \
	    jexec alcatraz pfctl -sn -a "*"
	atf_check -s exit:0 -o match:"rdr on ${epair}a inet proto tcp from any to any port = echo -> 127.0.0.1 port 7" \
	    jexec alcatraz pfctl -sn -a "*"
}

nat_cleanup()
{
	pft_cleanup
}

atf_test_case "include" "cleanup"
include_head()
{
	atf_set descr 'Test including inside anchors'
	atf_set require.user root
}

include_body()
{
	pft_init

	wd=`pwd`

	epair=$(vnet_mkepair)
	vnet_mkjail alcatraz ${epair}a

	ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	echo "pass" > ${wd}/extra.conf
	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "block" \
	    "anchor \"foo\" {\n\
	        include \"${wd}/extra.conf\"\n\
	    }"

	jexec alcatraz pfctl -sr

	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
}

include_cleanup()
{
	pft_cleanup
}

atf_test_case "quick" "cleanup"
quick_head()
{
	atf_set descr 'Test quick on anchors'
	atf_set require.user root
}

quick_body()
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
	    "anchor quick {\n\
	        pass\n\
	    }" \
	    "block"

	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1
	jexec alcatraz pfctl -sr -vv -a "*"
}

quick_cleanup()
{
	pft_cleanup
}

atf_test_case "recursive_flush" "cleanup"
recursive_flush_head()
{
	atf_set descr 'Test recursive flushing of rules'
	atf_set require.user root
}

recursive_flush_body()
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
	    "anchor \"foo\" {\n\
	        pass\n\
	    }"

	# We can ping thanks to the pass rule in foo
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# Only reset the main rules. I.e. not a recursive flush
	pft_set_rules alcatraz \
	    "block" \
	    "anchor \"foo\""

	# "foo" still has the pass rule, so this works
	jexec alcatraz pfctl -a "*" -sr
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.1

	# Now do a recursive flush
	atf_check -s exit:0 -e ignore -o ignore \
	    jexec alcatraz pfctl -a "*" -Fr
	pft_set_rules alcatraz \
	    "block" \
	    "anchor \"foo\""

	# So this fails
	jexec alcatraz pfctl -a "*" -sr
	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.1
}

recursive_flush_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "pr183198"
	atf_add_test_case "pr279225"
	atf_add_test_case "nested_anchor"
	atf_add_test_case "deeply_nested"
	atf_add_test_case "wildcard"
	atf_add_test_case "nested_label"
	atf_add_test_case "quick"
	atf_add_test_case "quick_nested"
	atf_add_test_case "counter"
	atf_add_test_case "nat"
	atf_add_test_case "include"
	atf_add_test_case "quick"
	atf_add_test_case "recursive_flush"
}
