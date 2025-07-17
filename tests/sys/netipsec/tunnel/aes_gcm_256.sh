
. $(atf_get_srcdir)/utils.subr

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'IPSec inet4 tunnel using aes-gcm-256'
	atf_set require.user root
}

v4_body()
{
	# Unload AESNI module if loaded
	kldstat -q -n aesni && kldunload aesni

	ist_test 4 aes-gcm-16 "123456789012345678901234567890123456"
}

v4_cleanup()
{
	ist_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'IPSec inet6 tunnel using aes-gcm-256'
	atf_set require.user root
}

v6_body()
{
	# Unload AESNI module if loaded
	kldstat -q -n aesni && kldunload aesni

	ist_test 6 aes-gcm-16 "123456789012345678901234567890123456"
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
