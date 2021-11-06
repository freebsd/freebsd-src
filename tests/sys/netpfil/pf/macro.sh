. $(atf_get_srcdir)/utils.subr

atf_test_case "nr" "cleanup"
nr_head()
{
	atf_set descr 'Test $nr expansion'
	atf_set require.user root
}

nr_body()
{
	# Ensure that when the optimiser collapses rules the macro expansion
	# has the correct rule number
	pft_init

	vnet_mkjail alcatraz
	jexec alcatraz ifconfig lo0 inet 127.0.0.1/8
	jexec alcatraz ifconfig lo0 inet 127.0.0.2/32 alias

	pft_set_rules alcatraz \
	    "pass quick on lo from lo:network to lo:network" \
	    "block quick all label \"ruleNo:\$nr\""

	no=$(jexec alcatraz pfctl -sr -vv | awk '/ruleNo/ { gsub("@", "", $1); print $1; }')
	ruleno=$(jexec alcatraz pfctl -sr -vv | awk '/ruleNo/ { gsub(/"ruleNo:/, "", $7); gsub(/"/, "", $7); print $7; }')
	if [ "${no}" -ne "${ruleno}" ];
	then
		atf_fail "Expected ruleNo $no != $ruleno"
	fi
}

nr_cleanup()
{
	pft_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "nr"
}
