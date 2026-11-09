#	$OpenBSD: sshfp-connect.sh,v 1.9 2026/07/16 09:07:18 dtucker Exp $
#	Placed in the Public Domain.

# This test requires external setup and thus is skipped unless
# TEST_SSH_SSHFP_DOMAIN is set.  It requires:
# 1) A DNSSEC-validating resolver such as unwind(8).
# 2) A DNSSEC-enabled domain, which TEST_SSH_SSHFP_DOMAIN points to,
#    containing he following SSHFP records with fingerprints from
#    rsa_openssh.pub in that domain that are expected to succeed:
#      rsa: valid sha1 and sha256 fingerprints.
#      rsa-sha{1,256}: valid fingerprints for that type only.
#    and the following records that are expected to fail:
#      rsa-bad: invalid sha1 fingerprint and good sha256 fingerprint
#      rsa-sha{1,256}-bad: invalid fingerprints for that type only.
#    The SSHFP records for the other key types follow the same pattern.
#    The BIND-format zone file $OBJ/sshfp-connect.zone is created
#      containing these records.

tid="sshfp connect"

if [ -z "${TEST_SSH_SSHFP_DOMAIN}" ]; then
	skip "TEST_SSH_SSHFP_DOMAIN not set."
fi

# Generate expected SSHFP zone file.  This can also be handy to import if
# you're setting this up from scratch.
for kt in `$SSH -Q key-plain | grep -v sk- | \
    egrep '^(ssh-rsa|ecdsa-sha2|ssh-ed25519)'`; do
	case "$kt" in
	ssh-rsa)		dnsname=rsa ;;
	ecdsa-sha2-nistp256)	dnsname=ecdsa256 ;;
	ecdsa-sha2-nistp384)	dnsname=ecdsa384 ;;
	ecdsa-sha2-nistp521)	dnsname=ecdsa521 ;;
	ssh-ed25519)		dnsname=ed25519 ;;
	*)			fatal "unknown keytype $kt" ;;
	esac
	file="${dnsname}_openssh"
	# Make good fingerprints
	$SSHKEYGEN -r ${dnsname} -f ${SRC}/${file}.pub
	$SSHKEYGEN -r ${dnsname}-sha1 -f ${SRC}/${file}.pub -Ohashalg=sha1
	$SSHKEYGEN -r ${dnsname}-sha256 -f ${SRC}/${file}.pub -Ohashalg=sha256
	# Make bad fingerprints.
	# For the name with both types we only want the sha1 to be bad.
	$SSHKEYGEN -r ${dnsname}-bad -f ${SRC}/${file}.pub -Ohashalg=sha1 | tr f e
	$SSHKEYGEN -r ${dnsname}-bad -f ${SRC}/${file}.pub -Ohashalg=sha256
	$SSHKEYGEN -r ${dnsname}-sha1-bad -f ${SRC}/${file}.pub -Ohashalg=sha1 | tr f e
	$SSHKEYGEN -r ${dnsname}-sha256-bad -f ${SRC}/${file}.pub -Ohashalg=sha256 | tr f e
done | sort -n -k4,5 > $OBJ/sshfp-connect.zone

# Check that the required DNS entries exist.
# This also primes any DNS caches and resolvers.
while read line; do
	name=`echo "$line" | awk '{print $1}'`
	expected=`echo "$line" | awk '{print $4" "$5" "$6}' | tr a-z A-Z`
	# Ensure at least one result matches exactly
	matched=no

	# This uses a here document instead of "foo | while read" since the
	# the latter runs the loop inside a subshell and any variables set
	# vanish when the subshell does.
	while read result; do
		if [ "$result" = "$expected" ]; then
			matched=yes
		fi
	done <<EOD
`host -t sshfp "${name}.${TEST_SSH_SSHFP_DOMAIN}" | \
    awk '{print $5" "$6" "$7$8}' | tr a-z A-Z`
EOD
	if [ "$matched" = "no" ]; then
		fatal "$name.${TEST_SSH_SSHFP_DOMAIN} SSHFP record does not match required"
	fi
	trace "verified sshfp record '$name' -> '$expected'"
done <${OBJ}/sshfp-connect.zone
verbose "all required sshfp entries exist"

# Zero out known hosts and key aliases to force use of SSHFP records.
> $OBJ/known_hosts
mv $OBJ/ssh_proxy $OBJ/ssh_proxy.orig
sed -e "/HostKeyAlias.*localhost-with-alias/d" \
    -e "/Hostname.*127.0.0.1/d" \
    $OBJ/ssh_proxy.orig > $OBJ/ssh_proxy

opts="-F $OBJ/ssh_proxy -oVerifyHostKeyDNS=yes"

if $SSH -Q key-plain | grep ssh-rsa >/dev/null; then
	verbose "connect sshfp rsa"

	# Set RSA host key to match fingerprints above.
	mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
	$SUDO cp $SRC/rsa_openssh.prv $OBJ/host.ssh-rsa
	$SUDO chmod 600 $OBJ/host.ssh-rsa
	sed -e "s|$OBJ/ssh-rsa|$OBJ/host.ssh-rsa|" \
	    $OBJ/sshd_proxy.orig > $OBJ/sshd_proxy

	for n in rsa rsa-sha1 rsa-sha256; do
		trace "sshfp connect $n good fingerprint"
		algs="$opts -oHostKeyAlgorithms=rsa-sha2-512,rsa-sha2-256"
		host="${n}.${TEST_SSH_SSHFP_DOMAIN}"
		SSH_CONNECTION=`${SSH} $opts $algs $host 'echo $SSH_CONNECTION'`
		if [ $? -ne 0 ]; then
			fail "ssh sshfp connect $n failed"
		fi
		if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
			fail "bad SSH_CONNECTION: $SSH_CONNECTION"
		fi

		trace "sshfp connect $n bad fingerprint"
		host="${n}-bad.${TEST_SSH_SSHFP_DOMAIN}"
		if ${SSH} $opts $algs ${host} true 2>/dev/null; then
			fail "sshfp-connect succeeded with bad SSHFP record"
		fi
	done
fi

if $SSH -Q key-plain | grep ecdsa-sha2-nistp >/dev/null; then
    for b in 256 384 521; do
	verbose "connect sshfp ecdsa${b}"

	# Set ecdsa host key to match fingerprints above.
	mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
	$SUDO cp $SRC/ecdsa${b}_openssh.prv $OBJ/host.ecdsa-sha2-nistp${b}
	$SUDO chmod 600 $OBJ/host.ecdsa-sha2-nistp${b}
	sed -e "s|$OBJ/ecdsa-sha2-nistp${b}|$OBJ/host.ecdsa-sha2-nistp${b}|" \
	    $OBJ/sshd_proxy.orig > $OBJ/sshd_proxy

	for n in ecdsa${b} ecdsa${b}-sha1 ecdsa${b}-sha256; do
		trace "sshfp connect $n good fingerprint"
		algs="$opts -oHostKeyAlgorithms=ecdsa-sha2-nistp${b}"
		host="${n}.${TEST_SSH_SSHFP_DOMAIN}"
		SSH_CONNECTION=`${SSH} $opts $algs $host 'echo $SSH_CONNECTION'`
		if [ $? -ne 0 ]; then
			fail "ssh sshfp connect $n failed"
		fi
		if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
			fail "bad SSH_CONNECTION: $SSH_CONNECTION"
		fi

		trace "sshfp connect $n bad fingerprint"
		host="${n}-bad.${TEST_SSH_SSHFP_DOMAIN}"
		if ${SSH} $opts $algs ${host} true 2>/dev/null; then
			fail "sshfp-connect succeeded with bad SSHFP record"
		fi
	done
    done
fi

if $SSH -Q key-plain | grep ssh-ed25519 >/dev/null; then
	verbose "connect sshfp ed25519"

	# Set ed25519 host key to match fingerprints above.
	mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
	$SUDO cp $SRC/ed25519_openssh.prv $OBJ/host.ssh-ed25519
	$SUDO chmod 600 $OBJ/host.ssh-ed25519
	sed -e "s|$OBJ/ssh-ed25519|$OBJ/host.ssh-ed25519|" \
	    $OBJ/sshd_proxy.orig > $OBJ/sshd_proxy

	for n in ed25519 ed25519-sha1 ed25519-sha256; do
		trace "sshfp connect $n good fingerprint"
		algs="$opts -oHostKeyAlgorithms=ssh-ed25519"
		host="${n}.${TEST_SSH_SSHFP_DOMAIN}"
		SSH_CONNECTION=`${SSH} $opts $algs $host 'echo $SSH_CONNECTION'`
		if [ $? -ne 0 ]; then
			fail "ssh sshfp connect $n failed"
		fi
		if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
			fail "bad SSH_CONNECTION: $SSH_CONNECTION"
		fi

		trace "sshfp connect $n bad fingerprint"
		host="${n}-bad.${TEST_SSH_SSHFP_DOMAIN}"
		if ${SSH} $opts $algs ${host} true 2>/dev/null; then
			fail "sshfp-connect succeeded with bad SSHFP record"
		fi
	done
fi
