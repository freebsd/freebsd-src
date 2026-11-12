. $(atf_get_srcdir)/utils.subr

# The explicit IV of AES-GCM, AES-CTR and ChaCha20-Poly1305 must never repeat
# under one key.  esp_output() takes it from a per-SA counter that every CPU
# sending on the SA advances, so drive one tunnel from several processes at
# once, capture the ESP on the wire and look for a repeated IV per SPI.

atf_test_case "unique" "cleanup"
unique_head()
{
	atf_set descr 'The explicit ESP IV is unique per SA under concurrent senders'
	atf_set require.user root
	atf_set require.progs tcpdump
	atf_set timeout 600
}

unique_body()
{
	if [ "$(sysctl -n hw.ncpu)" -lt 2 ]; then
		atf_skip "This test needs more than one CPU"
	fi

	ist_init
	ist_labsetup
	ist_v4_setup
	ist_setkey ipsecA out 4 aes-gcm-16 "12345678901234567890"
	atf_check -s exit:0 -o ignore \
	    jexec ipsecA setkey -f ${TMPDIR}/ipsec.ipsecA.conf
	ist_setkey ipsecB in 4 aes-gcm-16 "12345678901234567890"
	atf_check -s exit:0 -o ignore \
	    jexec ipsecB setkey -f ${TMPDIR}/ipsec.ipsecB.conf
	atf_check -s exit:0 -o ignore jexec hostA ping -c 1 203.0.113.1

	# ESP in both directions crosses the router.
	jexec router tcpdump -ni ${epair_PUB_A}b -s 64 -w ${TMPDIR}/esp.pcap esp \
	    > /dev/null 2>&1 &
	tcpdump_pid=$!
	sleep 1

	# Flood pings from the gateway itself, so that each one runs
	# esp_output() in its own context on whatever CPU the scheduler gives
	# it: many concurrent senders on the same SA each way.  (Sent from
	# hostA they would all arrive through one epair queue and be
	# serialised before they reach IPsec.)
	pids=""
	for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
		jexec ipsecA ping -q -f -c 12000 -S 192.0.2.2 203.0.113.1 \
		    > /dev/null 2>&1 &
		pids="${pids} $!"
	done
	wait ${pids}
	sleep 1
	kill -INT ${tcpdump_pid}
	wait ${tcpdump_pid} 2>/dev/null

	# Per packet: hex of the first 48 bytes after the link header; the ESP
	# header follows the IPv4 header, the explicit IV follows the 8-byte
	# ESP header.  Count a (SPI, IV) pair seen more than once.
	tcpdump -nnr ${TMPDIR}/esp.pcap -x esp 2>/dev/null | awk '
		/^[0-9][0-9]:/ { flush(); next }
		/^[ \t]*0x00[012]0:/ { line = $0; sub(/^[ \t]*0x00[0-9a-f]+:[ \t]*/, "", line); gsub(/ /, "", line); hex = hex line; next }
		function flush() {
			if (length(hex) >= 72) {
				ihl = strtonum_hex(substr(hex, 2, 1)) * 8;
				spi = substr(hex, ihl + 1, 8); iv = substr(hex, ihl + 17, 16);
				n++; if (++seen[spi " " iv] == 2) dup++;
			}
			hex = ""
		}
		function strtonum_hex(c) { return index("0123456789abcdef", c) - 1 }
		END { flush(); printf "%d %d\n", n, dup }' > ${TMPDIR}/esp.count
	read packets dups < ${TMPDIR}/esp.count
	echo "ESP packets captured: ${packets}, repeated IVs: ${dups}"

	if [ "${packets}" -lt 100000 ]; then
		atf_fail "Captured only ${packets} ESP packets"
	fi
	if [ "${dups}" -ne 0 ]; then
		atf_fail "${dups} ESP packets reused an explicit IV"
	fi
}

unique_cleanup()
{
	ist_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case "unique"
}
