#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Mark Johnston <markj@FreeBSD.org>
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

TABLE_STATS_ZERO_REGEXP='Packets: 0[[:space:]]*Bytes: 0[[:space:]]'
TABLE_STATS_NONZERO_REGEXP='Packets: [1-9][0-9]*[[:space:]]*Bytes: [1-9][0-9]*[[:space:]]'

atf_test_case "v4_counters" "cleanup"
v4_counters_head()
{
	atf_set descr 'Verify per-address counters for v4'
	atf_set require.user root
}

v4_counters_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "table <foo> counters { 192.0.2.1 }" \
	    "block all" \
	    "pass in from <foo> to any" \
	    "pass out from any to <foo>" \
	    "set skip on lo"

	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2

	atf_check -s exit:0 -e ignore \
	    -o match:'In/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'Out/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -T show -vv
}

v4_counters_cleanup()
{
	pft_cleanup
}

atf_test_case "v6_counters" "cleanup"
v6_counters_head()
{
	atf_set descr 'Verify per-address counters for v6'
	atf_set require.user root
}

v6_counters_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a inet6 2001:db8:42::1/64 up no_dad -ifdisabled

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b inet6 2001:db8:42::2/64 up no_dad
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "table <foo6> counters { 2001:db8:42::1 }" \
	    "block all" \
	    "pass in from <foo6> to any" \
	    "pass out from any to <foo6>" \
	    "set skip on lo"

	atf_check -s exit:0 -o ignore ping -6 -c 3 2001:db8:42::2

	atf_check -s exit:0 -e ignore \
	    -o match:'In/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'Out/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo6 -T show -vv
}

v6_counters_cleanup()
{
	pft_cleanup
}

atf_test_case "match_counters" "cleanup"
match_counters_head()
{
	atf_set descr 'Test that counters for tables in match rules work'
	atf_set require.user root
}

match_counters_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "table <foo> counters { 192.0.2.1 }" \
	    "pass all" \
	    "match in from <foo> to any" \
	    "match out from any to <foo>" \
	    "set skip on lo"

	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2

	atf_check -s exit:0 -e ignore \
	    -o match:'In/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'Out/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -T show -vv
}

match_counters_cleanup()
{
	pft_cleanup
}

atf_test_case "zero_one" "cleanup"
zero_one_head()
{
	atf_set descr 'Test zeroing a single address in a table'
	atf_set require.user root
}

pft_cleared_ctime()
{
	jexec "$1" pfctl -t "$2" -vvT show | awk -v ip="$3" '
	  ($1 == ip) { m = 1 }
	  ($1 == "Cleared:" && m) {
	    sub("[[:space:]]*Cleared:[[:space:]]*", ""); print; exit }'
}

ctime_to_unixtime()
{
	# NB: it's not TZ=UTC, it's TZ=/etc/localtime
	date -jf '%a %b %d %H:%M:%S %Y' "$1" '+%s'
}

zero_one_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up
	ifconfig ${epair_send}a inet alias 192.0.2.3/24

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "table <foo> counters { 192.0.2.1, 192.0.2.3 }" \
	    "block all" \
	    "pass in from <foo> to any" \
	    "pass out from any to <foo>" \
	    "set skip on lo"

	atf_check -s exit:0 -o ignore ping -c 3 -S 192.0.2.1 192.0.2.2
	atf_check -s exit:0 -o ignore ping -c 3 -S 192.0.2.3 192.0.2.2

	jexec alcatraz pfctl -t foo -T show -vv

	atf_check -s exit:0 -e ignore \
	    -o match:'In/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'Out/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -T show -vv

	local uniq base ts1 ts3
	uniq=`jexec alcatraz pfctl -t foo -vvT show | sort -u | grep -c Cleared`
	atf_check_equal 1 "$uniq" # time they were added

	base=`pft_cleared_ctime alcatraz foo 192.0.2.1`

	atf_check -s exit:0 -e ignore \
	    jexec alcatraz pfctl -t foo -T zero 192.0.2.3

	ts1=`pft_cleared_ctime alcatraz foo 192.0.2.1`
	atf_check_equal "$base" "$ts1"

	ts3=`pft_cleared_ctime alcatraz foo 192.0.2.3`
	atf_check test "$ts1" != "$ts3"

	ts1=`ctime_to_unixtime "$ts1"`
	ts3=`ctime_to_unixtime "$ts3"`
	atf_check test $(( "$ts3" - "$ts1" )) -lt 10 # (3 pings * 2) + epsilon
	atf_check test "$ts1" -lt "$ts3"

	# We now have a zeroed and a non-zeroed counter, so both patterns
	# should match
	atf_check -s exit:0 -e ignore \
	    -o match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -T show -vv
}

zero_one_cleanup()
{
	pft_cleanup
}

atf_test_case "reset_nonzero" "cleanup"
reset_nonzero_head()
{
	atf_set descr 'Test zeroing an address with non-zero counters'
	atf_set require.user root
}

reset_nonzero_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up
	ifconfig ${epair_send}a inet alias 192.0.2.3/24

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "table <foo> counters { 192.0.2.1, 192.0.2.3 }" \
	    "table <bar> counters { }" \
	    "block all" \
	    "pass in from <foo> to any" \
	    "pass out from any to <foo>" \
	    "pass on notReallyAnIf from <bar> to <bar>" \
	    "set skip on lo"

	# Nonexisting table can't be reset, following `-T show`.
	atf_check -o ignore \
	    -s not-exit:0 \
	    -e inline:"pfctl: Table does not exist.\n" \
	    jexec alcatraz pfctl -t nonexistent -T reset

	atf_check -o ignore \
	    -s exit:0 \
	    -e inline:"0/0 stats cleared.\n" \
	    jexec alcatraz pfctl -t bar -T reset

	# No-op is a valid operation.
	atf_check -s exit:0 \
	    -e inline:"0/2 stats cleared.\n" \
	    jexec alcatraz pfctl -t foo -T reset

	atf_check -s exit:0 -o ignore ping -c 3 -S 192.0.2.3 192.0.2.2

	atf_check -s exit:0 -e ignore \
	    -o match:'In/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -vvT show

	local clrd uniq
	clrd=`jexec alcatraz pfctl -t foo -vvT show | grep -c Cleared`
	uniq=`jexec alcatraz pfctl -t foo -vvT show | sort -u | grep -c Cleared`
	atf_check_equal "$clrd" 2
	atf_check_equal "$uniq" 1 # time they were added

	atf_check -s exit:0 -e ignore \
	    -e inline:"1/2 stats cleared.\n" \
	    jexec alcatraz pfctl -t foo -T reset

	clrd=`jexec alcatraz pfctl -t foo -vvT show | grep -c Cleared`
	uniq=`jexec alcatraz pfctl -t foo -vvT show | sort -u | grep -c Cleared`
	atf_check_equal "$clrd" 2
	atf_check_equal "$uniq" 2 # 192.0.2.3 should get new timestamp

	atf_check -s exit:0 -e ignore \
	    -o not-match:'In/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o not-match:'Out/Pass:.*'"$TABLE_STATS_NONZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -vvT show
}

reset_nonzero_cleanup()
{
	pft_cleanup
}

atf_test_case "pr251414" "cleanup"
pr251414_head()
{
	atf_set descr 'Test PR 251414'
	atf_set require.user root
}

pr251414_body()
{
	pft_init

	epair_send=$(vnet_mkepair)
	ifconfig ${epair_send}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair_send}b
	jexec alcatraz ifconfig ${epair_send}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
		"pass all" \
		"table <tab> { self }" \
		"pass in log to <tab>"

	pft_set_rules noflush alcatraz \
		"pass all" \
		"table <tab> counters { self }" \
		"pass in log to <tab>"

	atf_check -s exit:0 -o ignore ping -c 3 192.0.2.2

	jexec alcatraz pfctl -t tab -T show -vv
}

pr251414_cleanup()
{
	pft_cleanup
}

atf_test_case "automatic" "cleanup"
automatic_head()
{
	atf_set descr "Test automatic - optimizer generated - tables"
	atf_set require.user root
}

automatic_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
		"block in" \
		"pass in proto icmp from 192.0.2.1" \
		"pass in proto icmp from 192.0.2.3" \
		"pass in proto icmp from 192.0.2.4" \
		"pass in proto icmp from 192.0.2.5" \
		"pass in proto icmp from 192.0.2.6" \
		"pass in proto icmp from 192.0.2.7" \
		"pass in proto icmp from 192.0.2.8" \
		"pass in proto icmp from 192.0.2.9"

	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
}

automatic_cleanup()
{
	pft_cleanup
}

atf_test_case "network" "cleanup"
network_head()
{
	atf_set descr 'Test <ifgroup>:network'
	atf_set require.user root
}

network_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
		"table <allow> const { epair:network }"\
		"block in" \
		"pass in from <allow>"

	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2
}

network_cleanup()
{
	pft_cleanup
}

atf_test_case "pr259689" "cleanup"
pr259689_head()
{
	atf_set descr 'Test PR 259689'
	atf_set require.user root
}

pr259689_body()
{
	pft_init

	vnet_mkjail alcatraz
	jexec alcatraz pfctl -e

	pft_set_rules alcatraz \
	    "pass in" \
	    "block in inet from { 1.1.1.1, 1.1.1.2, 2.2.2.2, 2.2.2.3, 4.4.4.4, 4.4.4.5 }"

	atf_check -o match:'block drop in inet from <__automatic_.*:6> to any' \
	    -e ignore \
	    jexec alcatraz pfctl -sr -vv
}

pr259689_cleanup()
{
	pft_cleanup
}

atf_test_case "precreate" "cleanup"
precreate_head()
{
	atf_set descr 'Test creating a table without counters, then loading rules that add counters'
	atf_set require.user root
}

precreate_body()
{
	pft_init

	vnet_mkjail alcatraz

	jexec alcatraz pfctl -t foo -T add 192.0.2.1
	jexec alcatraz pfctl -t foo -T show

	pft_set_rules noflush alcatraz \
		"table <foo> counters persist" \
		"pass in from <foo>"

	# Expect all counters to be zero
	atf_check -s exit:0 -e ignore \
	    -o match:'In/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'In/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Block:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    -o match:'Out/Pass:.*'"$TABLE_STATS_ZERO_REGEXP" \
	    jexec alcatraz pfctl -t foo -T show -vv

}

precreate_cleanup()
{
	pft_cleanup
}

atf_test_case "anchor" "cleanup"
anchor_head()
{
	atf_set descr 'Test tables in anchors'
	atf_set require.user root
}

anchor_body()
{
	pft_init

	epair=$(vnet_mkepair)
	ifconfig ${epair}a 192.0.2.1/24 up

	vnet_mkjail alcatraz ${epair}b
	jexec alcatraz ifconfig ${epair}b 192.0.2.2/24 up
	jexec alcatraz pfctl -e

	(echo "table <testtable> persist"
	 echo "block in quick from <testtable> to any"
	) | jexec alcatraz pfctl -a anchorage -f -

	pft_set_rules noflush alcatraz \
		"pass" \
		"anchor anchorage"

	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# Tables belong to anchors, so this is a different table and won't affect anything
	jexec alcatraz pfctl -t testtable -T add 192.0.2.1
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# But when we add the address to the table in the anchor it does block traffic
	jexec alcatraz pfctl -a anchorage -t testtable -T add 192.0.2.1
	atf_check -s exit:2 -o ignore ping -c 1 192.0.2.2
}

anchor_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4_counters"
	atf_add_test_case "v6_counters"
	atf_add_test_case "match_counters"
	atf_add_test_case "zero_one"
	atf_add_test_case "reset_nonzero"
	atf_add_test_case "pr251414"
	atf_add_test_case "automatic"
	atf_add_test_case "network"
	atf_add_test_case "pr259689"
	atf_add_test_case "precreate"
	atf_add_test_case "anchor"
}
