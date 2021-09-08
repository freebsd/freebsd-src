#	$OpenBSD: sshfp-connect.sh,v 1.2 2021/07/19 08:48:33 dtucker Exp $
#	Placed in the Public Domain.

# This test requires external setup and thus is skipped unless
# TEST_SSH_SSHFP_DOMAIN is set.  It requires:
# 1) A DNSSEC-enabled domain, which TEST_SSH_SSHFP_DOMAIN points to.
# 2) A DNSSEC-validating resolver such as unwind(8).
# 3) The following SSHFP records with fingerprints from rsa_openssh.pub
#    in that domain that are expected to succeed:
#      sshtest: valid sha1 and sha256 fingerprints.
#      sshtest-sha{1,256}, : valid fingerprints for that type only.
#    and the following records that are expected to fail:
#      sshtest-bad: invalid sha1 fingerprint and good sha256 fingerprint
#      sshtest-sha{1,256}-bad: invalid fingerprints for that type only.
#
# sshtest IN SSHFP 1 1 99C79CC09F5F81069CC017CDF9552CFC94B3B929
# sshtest IN SSHFP 1 2 E30D6B9EB7A4DE495324E4D5870B8220577993EA6AF417E8E4A4F1C5 BF01A9B6
# sshtest-sha1 IN SSHFP 1 1 99C79CC09F5F81069CC017CDF9552CFC94B3B929
# sshtest-sha256 IN SSHFP 1 2 E30D6B9EB7A4DE495324E4D5870B8220577993EA6AF417E8E4A4F1C5 BF01A9B6
# sshtest-bad IN SSHFP 1 2 E30D6B9EB7A4DE495324E4D5870B8220577993EA6AF417E8E4A4F1C5 BF01A9B6
# sshtest-bad IN SSHFP 1 1 99C79CC09F5F81069CC017CDF9552CFC94B3B928
# sshtest-sha1-bad IN SSHFP 1 1 99D79CC09F5F81069CC017CDF9552CFC94B3B929
# sshtest-sha256-bad IN SSHFP 1 2 E30D6B9EB7A4DE495324E4D5870B8220577993EA6AF417E8E4A4F1C5 BF01A9B5

tid="sshfp connect"

if [ ! -z "${TEST_SSH_SSHFP_DOMAIN}" ] && \
    $SSH -Q key-plain | grep ssh-rsa >/dev/null; then

	# Set RSA host key to match fingerprints above.
	mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
	$SUDO cp $SRC/rsa_openssh.prv $OBJ/host.ssh-rsa
	$SUDO chmod 600 $OBJ/host.ssh-rsa
	sed -e "s|$OBJ/ssh-rsa|$OBJ/host.ssh-rsa|" \
	    $OBJ/sshd_proxy.orig > $OBJ/sshd_proxy

	# Zero out known hosts and key aliases to force use of SSHFP records.
	> $OBJ/known_hosts
	mv $OBJ/ssh_proxy $OBJ/ssh_proxy.orig
	sed -e "/HostKeyAlias.*localhost-with-alias/d" \
	    -e "/Hostname.*127.0.0.1/d" \
	    $OBJ/ssh_proxy.orig > $OBJ/ssh_proxy

	for n in sshtest sshtest-sha1 sshtest-sha256; do
		trace "sshfp connect $n good fingerprint"
		host="${n}.dtucker.net"
		opts="-F $OBJ/ssh_proxy -o VerifyHostKeyDNS=yes "
		opts="$opts -o HostKeyAlgorithms=ssh-rsa"
		host="${n}.${TEST_SSH_SSHFP_DOMAIN}"
		SSH_CONNECTION=`${SSH} $opts $host 'echo $SSH_CONNECTION'`
		if [ $? -ne 0 ]; then
			fail "ssh sshfp connect failed"
		fi
		if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
			fail "bad SSH_CONNECTION: $SSH_CONNECTION"
		fi

		trace "sshfp connect $n bad fingerprint"
		host="${n}-bad.${TEST_SSH_SSHFP_DOMAIN}"
		if ${SSH} $opts ${host} true; then
			fail "sshfp-connect succeeded with bad SSHFP record"
		fi
	done
else
	echo SKIPPED: TEST_SSH_SSHFP_DOMAIN not set.
fi
