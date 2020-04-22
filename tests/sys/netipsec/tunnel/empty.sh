# $FreeBSD$

. $(atf_get_srcdir)/utils.subr

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'IPSec inet4 tunnel using NULL encryption'
	atf_set require.user root
}

v4_body()
{
	if [ "$(atf_config_get ci false)" = "true" ]; then
		atf_skip "https://bugs.freebsd.org/245832"
	fi

	# Can't use filename "null" for this script: PR 223564
	ist_test 4 null ""
}

v4_cleanup()
{
	ist_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'IPSec inet6 tunnel using NULL encryption'
	atf_set require.user root
}

v6_body()
{
	if [ "$(atf_config_get ci false)" = "true" ]; then
		atf_skip "https://bugs.freebsd.org/245832"
	fi

	ist_test 6 null ""
}

v6_cleanup()
{
	ist_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4"
	atf_add_test_case "v6"
}
