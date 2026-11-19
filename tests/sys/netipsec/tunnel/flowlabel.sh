
. $(atf_get_srcdir)/utils.subr

# The outer IPv6 header of a tunnel mode SA carries the low 20 bits of the
# SA's flow ID, jenkins_hash32() of the SPI in host byte order with seed 0,
# as its flow label.  Expected labels for the SPIs used below:
#   0x1000 -> 0x625cd   0x1001 -> 0xead43   0x1002 -> 0x828ed
#   0x1003 -> 0xb1c69   0x1275 -> 0 (the smallest SPI above 255 that hashes
#   to a zero label)
GW_A=2001:db8:23::2
GW_B=2001:db8:34::2
KEY="12345678901234567890"
AKEY="12345678901234567890123456789012"

# fl_setkey JAIL DIR SRC_LAN DST_LAN PROTO SPI_OUT SPI_IN
fl_setkey()
{
	jname=$1; dir=$2; src=$3; dst=$4; proto=$5; spi_out=$6; spi_in=$7
	if [ ${proto} = esp ]; then
		alg="-E aes-gcm-16 \"${KEY}\""
	else
		alg="-A hmac-sha2-256 \"${AKEY}\""
	fi
	[ ${dir} = out ] && rdir=in || rdir=out
	(
		printf "flush;\nspdflush;\n"
		printf "spdadd ${src} ${dst} any -P ${dir} ipsec ${proto}/tunnel/${GW_A}-${GW_B}/require;\n"
		printf "spdadd ${dst} ${src} any -P ${rdir} ipsec ${proto}/tunnel/${GW_B}-${GW_A}/require;\n"
		printf "add ${GW_A} ${GW_B} ${proto} ${spi_out} ${alg};\n"
		printf "add ${GW_B} ${GW_A} ${proto} ${spi_in} ${alg};\n"
	) > ${TMPDIR}/ipsec.${jname}.conf
	atf_check -s exit:0 -o ignore jexec ${jname} setkey -f ${TMPDIR}/ipsec.${jname}.conf
}

# fl_run COUNT TARGET...: ping each target from hostA while capturing COUNT
# packets that match fl_filter on ipsecA's public interface, by default ESP
# or AH in both directions.  A test that expects no replies sets
# fl_ping_status=ignore.
fl_ping_status=exit:0
fl_filter="ip6 and (esp or ah)"
fl_run()
{
	count=$1; shift
	rm -f ${TMPDIR}/fl.pcap ${TMPDIR}/tcpdump.err
	jexec ipsecA tcpdump -i ${epair_PUB_A}a -n -c ${count} \
	    -w ${TMPDIR}/fl.pcap "${fl_filter}" \
	    > /dev/null 2> ${TMPDIR}/tcpdump.err &
	tdpid=$!
	# tcpdump reports the interface once the capture is open.
	i=0
	while ! grep -q "listening on" ${TMPDIR}/tcpdump.err 2>/dev/null; do
		if ! kill -0 ${tdpid} 2>/dev/null || [ ${i} -ge 50 ]; then
			cat ${TMPDIR}/tcpdump.err
			atf_fail "tcpdump did not start capturing"
		fi
		sleep 0.1
		i=$((i + 1))
	done
	for target in "$@"; do
		atf_check -s ${fl_ping_status} -o ignore \
		    jexec hostA ping -c 5 -i 0.2 ${target}
	done
	timeout 5 sh -c "while kill -0 ${tdpid} 2>/dev/null; do sleep 0.2; done"
	kill ${tdpid} 2>/dev/null
	wait ${tdpid} 2>/dev/null
}

# fl_check SPI LABEL: every captured packet of SPI carries LABEL, a hex
# string without 0x; "0" means no flow label at all.
fl_check()
{
	spi=$(printf "%08x" $1); label=$2
	pk=$(tcpdump -n -v -r ${TMPDIR}/fl.pcap 2>/dev/null | grep "spi=0x${spi}")
	n=$(echo "${pk}" | grep -c "spi=0x${spi}")
	if [ ${n} -lt 5 ]; then
		cat ${TMPDIR}/tcpdump.err
		atf_fail "only ${n} packets with SPI 0x${spi} captured"
	fi
	if [ ${label} = 0 ]; then
		bad=$(echo "${pk}" | grep -c "flowlabel")
	else
		bad=$(echo "${pk}" | grep -vc "flowlabel 0x${label},")
	fi
	if [ ${bad} -ne 0 ]; then
		echo "${pk}"
		atf_fail "${bad} of ${n} packets with SPI 0x${spi} lack flow label ${label}"
	fi
}

fl_lab()
{
	ist_init
	ist_labsetup
	[ $1 = 4 ] && ist_v4_setup
	ist_v6_setup
}

atf_test_case "v4in6" "cleanup"
v4in6_head()
{
	atf_set descr 'Outer IPv6 flow label of an IPv4-in-IPv6 ESP tunnel is the SA hash'
	atf_set require.user root
}
v4in6_body()
{
	fl_lab 4
	fl_setkey ipsecA out 192.0.2.0/24 203.0.113.0/24 esp 0x1000 0x1001
	fl_setkey ipsecB in 192.0.2.0/24 203.0.113.0/24 esp 0x1000 0x1001
	fl_run 10 203.0.113.1
	fl_check 0x1000 625cd
	fl_check 0x1001 ead43
}
v4in6_cleanup()
{
	ist_cleanup
}

atf_test_case "v6in6" "cleanup"
v6in6_head()
{
	atf_set descr 'Outer IPv6 flow label of an IPv6-in-IPv6 ESP tunnel is the SA hash'
	atf_set require.user root
}
v6in6_body()
{
	fl_lab 6
	fl_setkey ipsecA out 2001:db8:1::/64 2001:db8:45::/64 esp 0x1000 0x1001
	fl_setkey ipsecB in 2001:db8:1::/64 2001:db8:45::/64 esp 0x1000 0x1001
	fl_run 10 2001:db8:45::1
	fl_check 0x1000 625cd
	fl_check 0x1001 ead43
}
v6in6_cleanup()
{
	ist_cleanup
}

atf_test_case "ah" "cleanup"
ah_head()
{
	atf_set descr 'An AH tunnel authenticates with the flow label set'
	atf_set require.user root
}
ah_body()
{
	# IPv4 inner: an IPv6-in-IPv6 AH tunnel returns no traffic on an
	# unpatched kernel either, so it cannot serve here.
	fl_lab 4
	fl_setkey ipsecA out 192.0.2.0/24 203.0.113.0/24 ah 0x1002 0x1003
	fl_setkey ipsecB in 192.0.2.0/24 203.0.113.0/24 ah 0x1002 0x1003
	fl_run 10 203.0.113.1
	fl_check 0x1002 828ed
	fl_check 0x1003 b1c69
}
ah_cleanup()
{
	ist_cleanup
}

atf_test_case "zero" "cleanup"
zero_head()
{
	atf_set descr 'An SPI whose hash has a zero low 20 bits yields no flow label'
	atf_set require.user root
}
zero_body()
{
	fl_lab 6
	fl_setkey ipsecA out 2001:db8:1::/64 2001:db8:45::/64 esp 0x1275 0x1001
	fl_setkey ipsecB in 2001:db8:1::/64 2001:db8:45::/64 esp 0x1275 0x1001
	fl_run 10 2001:db8:45::1
	fl_check 0x1275 0
	fl_check 0x1001 ead43
}
zero_cleanup()
{
	ist_cleanup
}

atf_test_case "sa_flowid_off" "cleanup"
sa_flowid_off_head()
{
	atf_set descr 'The flow label does not depend on net.inet.ipsec.sa_flowid'
	atf_set require.user root
}
sa_flowid_off_body()
{
	fl_lab 6
	atf_check -s exit:0 -o ignore jexec ipsecA sysctl net.inet.ipsec.sa_flowid=0
	fl_setkey ipsecA out 2001:db8:1::/64 2001:db8:45::/64 esp 0x1000 0x1001
	fl_setkey ipsecB in 2001:db8:1::/64 2001:db8:45::/64 esp 0x1000 0x1001
	fl_run 10 2001:db8:45::1
	fl_check 0x1000 625cd
}
sa_flowid_off_cleanup()
{
	ist_cleanup
}

# fl_bundle [AH_MODE]: two ESP tunnel SAs, one per policy, feed one AH
# transport SA, added with setkey's -m AH_MODE (none: setkey's default
# mode "any").  The outer header must carry the label of the AH SA on every
# packet, not that of the tunnel SA which built it.  AH leaves the ESP
# header readable, so the capture shows which tunnel SA each packet took.
# Only the outbound packets carry AH, so only they are captured: when this
# was written, ipsec6_common_input_cb() handed the ESP header beneath a
# transport mode AH to the raw IPv6 input, so ipsecB did not decapsulate
# the packets and did not reply.
fl_bundle()
{
	ahmode=${1:+-m $1}
	fl_lab 4
	(
		printf "flush;\nspdflush;\n"
		printf "spdadd 192.0.2.0/24 203.0.113.0/25 any -P out ipsec esp/tunnel/${GW_A}-${GW_B}/unique:1 ah/transport//require;\n"
		printf "spdadd 192.0.2.0/24 203.0.113.128/25 any -P out ipsec esp/tunnel/${GW_A}-${GW_B}/unique:2 ah/transport//require;\n"
		printf "spdadd 203.0.113.0/24 192.0.2.0/24 any -P in ipsec esp/tunnel/${GW_B}-${GW_A}/require;\n"
		printf "add ${GW_A} ${GW_B} esp 0x1000 -m tunnel -u 1 -E aes-gcm-16 \"${KEY}\";\n"
		printf "add ${GW_A} ${GW_B} esp 0x1001 -m tunnel -u 2 -E aes-gcm-16 \"${KEY}\";\n"
		printf "add ${GW_A} ${GW_B} ah 0x1002 ${ahmode} -A hmac-sha2-256 \"${AKEY}\";\n"
		printf "add ${GW_B} ${GW_A} esp 0x1003 -m tunnel -E aes-gcm-16 \"${KEY}\";\n"
	) > ${TMPDIR}/ipsec.ipsecA.conf
	atf_check -s exit:0 -o ignore jexec ipsecA setkey -f ${TMPDIR}/ipsec.ipsecA.conf
	(
		printf "flush;\nspdflush;\n"
		printf "spdadd 192.0.2.0/24 203.0.113.0/24 any -P in ipsec esp/tunnel/${GW_A}-${GW_B}/require;\n"
		printf "spdadd 203.0.113.0/24 192.0.2.0/24 any -P out ipsec esp/tunnel/${GW_B}-${GW_A}/require;\n"
		printf "add ${GW_A} ${GW_B} esp 0x1000 -m tunnel -E aes-gcm-16 \"${KEY}\";\n"
		printf "add ${GW_A} ${GW_B} esp 0x1001 -m tunnel -E aes-gcm-16 \"${KEY}\";\n"
		printf "add ${GW_A} ${GW_B} ah 0x1002 ${ahmode} -A hmac-sha2-256 \"${AKEY}\";\n"
		printf "add ${GW_B} ${GW_A} esp 0x1003 -m tunnel -E aes-gcm-16 \"${KEY}\";\n"
	) > ${TMPDIR}/ipsec.ipsecB.conf
	atf_check -s exit:0 -o ignore jexec ipsecB setkey -f ${TMPDIR}/ipsec.ipsecB.conf
	fl_ping_status=ignore
	fl_filter="ip6 and ah"
	fl_run 10 203.0.113.1 203.0.113.129
	fl_check 0x1002 828ed
	fl_check 0x1000 828ed
	fl_check 0x1001 828ed
}

atf_test_case "bundle" "cleanup"
bundle_head()
{
	atf_set descr 'A bundle of tunnel and transport mode SAs carries the label of the outermost SA'
	atf_set require.user root
}
bundle_body()
{
	fl_bundle transport
}
bundle_cleanup()
{
	ist_cleanup
}

atf_test_case "bundle_any" "cleanup"
bundle_any_head()
{
	atf_set descr 'The bundle label follows the policy when the transport SA has mode any'
	atf_set require.user root
}
bundle_any_body()
{
	fl_bundle
}
bundle_any_cleanup()
{
	ist_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "v4in6"
	atf_add_test_case "v6in6"
	atf_add_test_case "ah"
	atf_add_test_case "zero"
	atf_add_test_case "sa_flowid_off"
	atf_add_test_case "bundle"
	atf_add_test_case "bundle_any"
}
