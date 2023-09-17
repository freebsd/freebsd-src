
. $(atf_get_srcdir)/../common/vnet.subr

atf_test_case "235704" "cleanup"
235704_head()
{
	atf_set descr "Test PR #235704"
	atf_set require.user root
}
235704_body()
{
	vnet_init
	vnet_mkjail one

	tun=$(jexec one ifconfig tun create)
	jexec one ifconfig ${tun} name foo
	atf_check -s exit:0 jexec one ifconfig foo destroy
}
235704_cleanup()
{
	vnet_cleanup
}

atf_test_case "basic" "cleanup"
basic_head()
{
	atf_set descr "Test if_tun using nc"
	atf_set require.user root
}
basic_body()
{
	vnet_init

	epair=$(vnet_mkepair)

	tun_duke=$(ifconfig tun create)
	tun_bass=$(ifconfig tun create)

	vnet_mkjail duke ${epair}a ${tun_duke}
	vnet_mkjail bass ${epair}b ${tun_bass}

	jexec duke ifconfig ${epair}a inet 10.0.0.1/24 up
	jexec bass ifconfig ${epair}b inet 10.0.0.2/24 up

	jexec duke nc -u -l --tun /dev/${tun_duke} 10.0.0.1 2600 &
	jexec bass nc -u --tun /dev/${tun_bass} 10.0.0.1 2600 &

	jexec duke ifconfig ${tun_duke} inet 10.100.0.1/24 10.100.0.2 up
	jexec bass ifconfig ${tun_bass} inet 10.100.0.2/24 10.100.0.1 up

	atf_check -s exit:0 -o ignore \
		jexec bass ping -c 1 10.100.0.1
}
basic_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "235704"
	atf_add_test_case "basic"
}
