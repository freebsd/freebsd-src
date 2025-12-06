#
# Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. $(atf_get_srcdir)/../../sys/common/vnet.subr

atf_test_case nptv6 cleanup
nptv6_head()
{
	atf_set "descr" "Test creation of NPTv6 rules"
	atf_set "require.user" "root"
	atf_set "require.kmods" "ipfw_nptv6"
}
nptv6_body()
{
	vnet_init
	local jail=ipfw_$(atf_get ident)
	local epair=$(vnet_mkepair)
	vnet_mkjail ${jail} ${epair}a

	local rule="xyzzy"
	local int="2001:db8:1::"
	local ext="2001:db8:2::"

	atf_check jexec ${jail} \
	    ifconfig "${epair}"a inet6 ${ext}1/64 up

	# This is how it's supposed to be used
	atf_check jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int} ext_prefix ${ext} prefixlen 64
	atf_check -o inline:\
"nptv6 $rule int_prefix $int ext_prefix $ext prefixlen 64\n" \
	    jexec ${jail} ipfw nptv6 all list
	atf_check jexec ${jail} ipfw nptv6 all destroy

	# Specify external interface rather than network
	atf_check jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int} ext_if ${epair}a prefixlen 64
	atf_check -o inline:\
"nptv6 $rule int_prefix $int ext_if ${epair}a prefixlen 64\n" \
	    jexec ${jail} ipfw nptv6 all list
	atf_check jexec ${jail} ipfw nptv6 all destroy

	# This should also work
	atf_check jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int}/64 ext_prefix ${ext}/64 prefixlen 64
	atf_check -o inline:\
"nptv6 $rule int_prefix $int ext_prefix $ext prefixlen 64\n" \
	    jexec ${jail} ipfw nptv6 all list
	atf_check jexec ${jail} ipfw nptv6 all destroy

	# This should also work, although it's not encouraged
	atf_check -e match:"use prefixlen instead" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int}/64 ext_prefix ${ext}/64
	atf_check -o inline:\
"nptv6 $rule int_prefix $int ext_prefix $ext prefixlen 64\n" \
	    jexec ${jail} ipfw nptv6 all list
	atf_check jexec ${jail} ipfw nptv6 all destroy

	# These should all fail
	atf_check -s not-exit:0 -e match:"one ext_prefix or ext_if" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int} ext_prefix ${ext} ext_if ${epair}a
	atf_check -o empty jexec ${jail} ipfw nptv6 all list

	atf_check -s not-exit:0 -e match:"one ext_prefix or ext_if" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int} ext_if ${epair}a ext_prefix ${ext}
	atf_check -o empty jexec ${jail} ipfw nptv6 all list

	atf_check -s not-exit:0 -e match:"prefix length mismatch" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int}/48 ext_prefix ${ext}/64
	atf_check -o empty jexec ${jail} ipfw nptv6 all list

	atf_check -s not-exit:0 -e match:"prefix length mismatch" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int}/64 ext_prefix ${ext}/64 prefixlen 48
	atf_check -o empty jexec ${jail} ipfw nptv6 all list

	atf_check -s not-exit:0 -e match:"prefix length mismatch" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int}/64 ext_prefix ${ext} prefixlen 48
	atf_check -o empty jexec ${jail} ipfw nptv6 all list

	atf_check -s not-exit:0 -e match:"prefix length mismatch" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int} ext_prefix ${ext}/64 prefixlen 48
	atf_check -o empty jexec ${jail} ipfw nptv6 all list

	atf_check -s not-exit:0 -e match:"prefix length mismatch" \
	    jexec ${jail} ipfw nptv6 ${rule} create \
	    int_prefix ${int}/64 ext_if ${epair}a prefixlen 48
	atf_check -o empty jexec ${jail} ipfw nptv6 all list
}
nptv6_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case nptv6
}
