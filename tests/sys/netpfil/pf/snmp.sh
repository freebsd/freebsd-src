#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Kristof Provost <kp@FreeBSD.org>
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

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr 'Basic pf_snmp test'
	atf_set require.user root
}

basic_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}b 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	# Start bsnmpd
	jexec alcatraz bsnmpd -c $(atf_get_srcdir)/bsnmpd.conf

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "pass"

	# Sanity check, and create state
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	# pf should be enabled
	atf_check -s exit:0 -o match:'pfStatusRunning.0 = true' \
	    bsnmpwalk -s public@192.0.2.1 -i pf_tree.def begemot
}

basic_cleanup()
{
	pft_cleanup
}

atf_test_case "table" "cleanup"
table_head()
{
	atf_set descr 'Test tables and pf_snmp'
	atf_set require.user root
}

table_body()
{
	pft_init

	epair=$(vnet_mkepair)

	ifconfig ${epair}b 192.0.2.2/24 up

	vnet_mkjail alcatraz ${epair}a
	jexec alcatraz ifconfig ${epair}a 192.0.2.1/24 up

	jexec alcatraz pfctl -e
	pft_set_rules alcatraz \
	    "table <foo> counters { 192.0.2.0/24 }" \
	    "pass in from <foo>"

	# Start bsnmpd after creating the table so we don't have to wait for
	# a refresh timeout
	jexec alcatraz bsnmpd -c $(atf_get_srcdir)/bsnmpd.conf

	# Sanity check, and create state
	atf_check -s exit:0 -o ignore \
	    ping -c 1 192.0.2.1

	# We should have one table
	atf_check -s exit:0 -o match:'pfTablesTblNumber.0 = 1' \
	    bsnmpwalk -s public@192.0.2.1 -i pf_tree.def begemot

	# We have the 'foo' table
	atf_check -s exit:0 -o match:'pfTablesTblDescr.* = foo' \
	    bsnmpwalk -s public@192.0.2.1 -i pf_tree.def pfTables

	# Which contains address 192.0.2.0/24
	atf_check -s exit:0 -o match:'pfTablesAddrNet.* = 192.0.2.0' \
	    bsnmpwalk -s public@192.0.2.1 -i pf_tree.def pfTables
	atf_check -s exit:0 -o match:'pfTablesAddrPrefix.* = 24' \
	    bsnmpwalk -s public@192.0.2.1 -i pf_tree.def pfTables

	# Give bsnmp time to refresh the table
	sleep 6
	# Expect non-zero packet count
	atf_check -s exit:0 -o match:'pfTablesAddrPktsInPass.* = [1-9][0-9]*' \
	    bsnmpwalk -s public@192.0.2.1 -i pf_tree.def pfTables
}

table_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "basic"
	atf_add_test_case "table"
}
