
. $(atf_get_srcdir)/utils.subr

atf_test_case "v4" "cleanup"
v4_head()
{
	atf_set descr 'IPSec inet4 tunnel using aes-cbc-256-hmac-sha2-256 and AESNI'
	atf_set require.user root
}

v4_body()
{
	# Unload AESNI module if loaded
	kldstat -q -n aesni && kldunload aesni

	ist_test 4 rijndael-cbc "12345678901234567890123456789012" hmac-sha2-256 "12345678901234567890123456789012"
}

v4_cleanup()
{
	ist_cleanup
}

atf_test_case "v6" "cleanup"
v6_head()
{
	atf_set descr 'IPSec inet6 tunnel using aes-cbc-256-hmac-sha2-256 and AESNI'
	atf_set require.user root
}

v6_body()
{
	# Unload AESNI module if loaded
	kldstat -q -n aesni && kldunload aesni

	ist_test 6 rijndael-cbc "12345678901234567890123456789012" hmac-sha2-256 "12345678901234567890123456789012"
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
