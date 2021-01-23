. $(atf_get_srcdir)/utils.subr

atf_test_case "hfsc" "cleanup"
hfsc_head()
{
	atf_set descr 'Basic HFSC test'
	atf_set require.user root
}

hfsc_body()
{
	altq_init
	is_altq_supported hfsc

	epair=$(vnet_mkepair)
	vnet_mkjail altq_hfsc ${epair}b

	ifconfig ${epair}a 192.0.2.1/24 up
	jexec altq_hfsc ifconfig ${epair}b 192.0.2.2/24 up

	# Sanity check
	atf_check -s exit:0 -o ignore ping -i .1 -c 3 -s 1200 192.0.2.2

	jexec altq_hfsc pfctl -e
	pft_set_rules altq_hfsc \
	    "altq on ${epair}b bandwidth 100b hfsc queue { default }" \
	    "queue default hfsc(default linkshare 80b)" \
	    "pass proto icmp "

	# single ping succeeds just fine
	atf_check -s exit:0 -o ignore ping -c 1 192.0.2.2

	# "Saturate the link"
	ping -i .1 -c 5 -s 1200 192.0.2.2

	# We should now be hitting the limits and get this packet dropped.
	atf_check -s exit:2 -o ignore ping -c 1 -s 1200 192.0.2.2
}

hfsc_cleanup()
{
	altq_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "hfsc"
}

