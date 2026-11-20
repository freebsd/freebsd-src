
. $(atf_get_srcdir)/utils.subr

KEY="12345678901234567890"

# bundle_use 4|6: ipsecA's outbound policy bundles a required ESP tunnel
# with an AH transport request at level "use", and only the ESP SAs exist.
# The AH request is skipped, so the packets must leave with ESP alone;
# ipsecB has a plain ESP tunnel and drops anything else.
bundle_use()
{
	af=$1
	if [ ${af} = 4 ]; then
		lan_a=192.0.2.0/30; lan_b=203.0.113.0/30; target=203.0.113.1
		gw_a=198.51.100.2; gw_b=198.51.100.6
	else
		lan_a=2001:db8:1::/64; lan_b=2001:db8:45::/64; target=2001:db8:45::1
		gw_a=2001:db8:23::2; gw_b=2001:db8:34::2
	fi
	sas="add ${gw_a} ${gw_b} esp 0x1000 -m tunnel -E aes-gcm-16 \"${KEY}\";
add ${gw_b} ${gw_a} esp 0x1001 -m tunnel -E aes-gcm-16 \"${KEY}\";"

	ist_init
	ist_labsetup
	ist_v${af}_setup
	cat > ${TMPDIR}/ipsec.ipsecA.conf <<-EOF
	flush;
	spdflush;
	spdadd ${lan_a} ${lan_b} any -P out ipsec esp/tunnel/${gw_a}-${gw_b}/require ah/transport//use;
	spdadd ${lan_b} ${lan_a} any -P in ipsec esp/tunnel/${gw_b}-${gw_a}/require;
	${sas}
	EOF
	cat > ${TMPDIR}/ipsec.ipsecB.conf <<-EOF
	flush;
	spdflush;
	spdadd ${lan_a} ${lan_b} any -P in ipsec esp/tunnel/${gw_a}-${gw_b}/require;
	spdadd ${lan_b} ${lan_a} any -P out ipsec esp/tunnel/${gw_b}-${gw_a}/require;
	${sas}
	EOF
	atf_check -s exit:0 -o ignore jexec ipsecA setkey -f ${TMPDIR}/ipsec.ipsecA.conf
	atf_check -s exit:0 -o ignore jexec ipsecB setkey -f ${TMPDIR}/ipsec.ipsecB.conf
	atf_check -s exit:0 -o ignore jexec hostA ping -c 3 -i 0.2 ${target}
}

atf_test_case "use_v4" "cleanup"
use_v4_head()
{
	atf_set descr 'A bundled request at level use without an SA is skipped (IPv4)'
	atf_set require.user root
}
use_v4_body()
{
	bundle_use 4
}
use_v4_cleanup()
{
	ist_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "use_v4"
}
