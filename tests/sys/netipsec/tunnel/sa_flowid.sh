
. $(atf_get_srcdir)/utils.subr

# Outbound packets of a tunnel mode SA carry the SA's flow ID,
# jenkins_hash32() of the SPI in host byte order with seed 0, in place of
# the inner packet's.  A lagg(4) with use_flowid picks its port from
# (flowid >> flowid_shift) % ports, so with two ports and the default shift
# of 16 the SAs below group as
#   0x1000 (a09625cd) and 0x1001 (e1fead43): bit 16 clear
#   0x1003 (a7bb1c69) and 0x1004 (1cdb9da5): bit 16 set
# while lagg's own header hash, restricted to layer 2, puts every SA on the
# same port.
#
# Each SA carries NFLOW inner flows, 172.16.K.1 .. 172.16.K.NFLOW to
# 172.16.K.254, which have flow IDs of their own: the kernel hashes locally
# generated packets once a multipath route exists.  The hash key is a
# constant, so the inner flows of every SA spread over both ports whenever
# their flow IDs reach the lagg.
NFLOW=8
KEY="12345678901234567890"

sf_setup()
{
	ist_init
	_vnet_check_req lagg
	epair0=$(vnet_mkepair)
	epair1=$(vnet_mkepair)
	vnet_mkjail a ${epair0}a ${epair1}a
	vnet_mkjail b ${epair0}b ${epair1}b
	for j in a b; do
		[ ${j} = a ] && s=a || s=b
		jexec ${j} ifconfig ${epair0}${s} up
		jexec ${j} ifconfig ${epair1}${s} up
		jexec ${j} ifconfig lagg0 create
		jexec ${j} ifconfig lagg0 laggproto loadbalance \
		    laggport ${epair0}${s} laggport ${epair1}${s}
		jexec ${j} ifconfig lo0 up
	done
	jexec a ifconfig lagg0 inet 10.0.0.1/24 up
	jexec b ifconfig lagg0 inet 10.0.0.2/24 up
	for k in 0 1 3 4; do
		n=1
		while [ ${n} -le ${NFLOW} ]; do
			jexec a ifconfig lo0 alias 172.16.${k}.${n}/32
			n=$((n + 1))
		done
		jexec b ifconfig lo0 alias 172.16.${k}.254/32
	done
	jexec a route -q add -net 172.16.0.0/16 10.0.0.2
	jexec b route -q add -net 172.16.0.0/16 10.0.0.1
	# Give the inner packets flow IDs: net.route.hash_outbound is read-only
	# and turns on with the first multipath route.  This one carries nothing.
	jexec a route -q add -net 192.0.2.0/24 10.0.0.2
	jexec a route -q add -net 192.0.2.0/24 10.0.0.3
	if [ "$(jexec a sysctl -n net.route.hash_outbound)" != 1 ]; then
		atf_skip "This test requires net.route.hash_outbound"
	fi
	for j in a b; do
		[ ${j} = a ] && dir=out || dir=in
		[ ${j} = a ] && rdir=in || rdir=out
		(
			printf "flush;\nspdflush;\n"
			for k in 0 1 3 4; do
				spi=$((0x1000 + k)); rspi=$((0x2000 + k))
				printf "add 10.0.0.1 10.0.0.2 esp ${spi} -m tunnel -u ${spi} -E aes-gcm-16 \"${KEY}\";\n"
				printf "add 10.0.0.2 10.0.0.1 esp ${rspi} -m tunnel -u ${rspi} -E aes-gcm-16 \"${KEY}\";\n"
				printf "spdadd 172.16.${k}.0/24 172.16.${k}.254/32 any -P ${dir} ipsec esp/tunnel/10.0.0.1-10.0.0.2/unique:${spi};\n"
				printf "spdadd 172.16.${k}.254/32 172.16.${k}.0/24 any -P ${rdir} ipsec esp/tunnel/10.0.0.2-10.0.0.1/unique:${rspi};\n"
			done
		) > ${TMPDIR}/ipsec.${j}.conf
		atf_check -s exit:0 -o ignore jexec ${j} setkey -f ${TMPDIR}/ipsec.${j}.conf
	done
	# Resolve ARP and bring the SAs up before counting.
	atf_check -s exit:0 -o ignore jexec a ping -c 1 10.0.0.2
	for k in 0 1 3 4; do
		atf_check -s exit:0 -o ignore jexec a ping -c 1 -S 172.16.${k}.1 172.16.${k}.254
	done
	# Header hashing alone (layer 2 only) cannot tell the SAs apart.
	jexec a ifconfig lagg0 use_flowid lagghash l2
}

# sf_opackets IF: packets jail a has sent on IF
sf_opackets()
{
	jexec a netstat -ibn -I $1 | awk '$3 ~ /^<Link/ { print $9 }'
}

# sf_send K: ping over every inner flow of SA 0x100K, 10 packets each, and
# set d0 and d1 to the packets that left on lagg port 0 and 1.  Not a
# command substitution: atf_fail must end the test.
sf_send()
{
	b0=$(sf_opackets ${epair0}a); b1=$(sf_opackets ${epair1}a)
	n=1
	while [ ${n} -le ${NFLOW} ]; do
		jexec a ping -q -c 10 -i 0.02 -S 172.16.$1.${n} 172.16.$1.254 \
		    > /dev/null 2>&1 ||
		    atf_fail "ping from 172.16.$1.${n} over SA 0x100$1 failed"
		n=$((n + 1))
	done
	d0=$(( $(sf_opackets ${epair0}a) - b0 )); d1=$(( $(sf_opackets ${epair1}a) - b1 ))
	echo "SA 0x100$1: ${d0} packets on port 0, ${d1} on port 1"
}

# sf_member K VAR: set VAR to the lagg port (0 or 1) that carries every
# inner flow of SA 0x100K.
sf_member()
{
	sf_send $1
	if [ ${d0} -ge $((10 * NFLOW)) ] && [ ${d1} -le 2 ]; then
		eval $2=0
	elif [ ${d1} -ge $((10 * NFLOW)) ] && [ ${d0} -le 2 ]; then
		eval $2=1
	else
		atf_fail "SA 0x100$1 did not use one port for all of its flows"
	fi
}

atf_test_case "select" "cleanup"
select_head()
{
	atf_set descr 'lagg(4) use_flowid selects the port of an SA from its flow ID'
	atf_set require.user root
}
select_body()
{
	sf_setup
	sf_member 0 m0; sf_member 1 m1; sf_member 3 m3; sf_member 4 m4
	echo "ports: 0x1000 -> ${m0}, 0x1001 -> ${m1}, 0x1003 -> ${m3}, 0x1004 -> ${m4}"
	if [ "${m0}" != "${m1}" ] || [ "${m3}" != "${m4}" ] || [ "${m0}" = "${m3}" ]; then
		atf_fail "SAs did not group by bit 16 of their flow ID"
	fi
}
select_cleanup()
{
	vnet_cleanup
}

atf_test_case "off" "cleanup"
off_head()
{
	atf_set descr 'With net.inet.ipsec.sa_flowid=0 packets keep the inner flow ID'
	atf_set require.user root
}
off_body()
{
	sf_setup
	atf_check -s exit:0 -o ignore jexec a sysctl net.inet.ipsec.sa_flowid=0
	for k in 0 1 3 4; do
		sf_send ${k}
		if [ ${d0} -lt 10 ] || [ ${d1} -lt 10 ]; then
			atf_fail "the inner flows of SA 0x100${k} did not spread over the ports"
		fi
	done
}
off_cleanup()
{
	vnet_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "select"
	atf_add_test_case "off"
}
