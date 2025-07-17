#	$OpenBSD: keygen-sshfp.sh,v 1.3 2023/02/10 05:06:03 djm Exp $
#	Placed in the Public Domain.

tid="keygen-sshfp"

trace "keygen fingerprints"
fp=`${SSHKEYGEN} -r test -f ${SRC}/ed25519_openssh.pub | \
    awk '$5=="1"{print $6}'`
if [ "$fp" != "8a8647a7567e202ce317e62606c799c53d4c121f" ]; then
	fail "keygen fingerprint sha1"
fi
fp=`${SSHKEYGEN} -r test -f ${SRC}/ed25519_openssh.pub | \
    awk '$5=="2"{print $6}'`
if [ "$fp" != \
    "54a506fb849aafb9f229cf78a94436c281efcb4ae67c8a430e8c06afcb5ee18f" ]; then
	fail "keygen fingerprint sha256"
fi

# Expect two lines of output without an explicit algorithm
fp=`${SSHKEYGEN} -r test -f ${SRC}/ed25519_openssh.pub | wc -l`
if [ $(($fp + 0)) -ne 2 ] ; then
	fail "incorrect number of SSHFP records $fp (expected 2)"
fi

# Test explicit algorithm selection
exp="test IN SSHFP 4 1 8a8647a7567e202ce317e62606c799c53d4c121f"
fp=`${SSHKEYGEN} -Ohashalg=sha1 -r test -f ${SRC}/ed25519_openssh.pub`
if [ "x$exp" != "x$fp" ] ; then
	fail "incorrect SHA1 SSHFP output"
fi

exp="test IN SSHFP 4 2 54a506fb849aafb9f229cf78a94436c281efcb4ae67c8a430e8c06afcb5ee18f"
fp=`${SSHKEYGEN} -Ohashalg=sha256 -r test -f ${SRC}/ed25519_openssh.pub`
if [ "x$exp" != "x$fp" ] ; then
	fail "incorrect SHA256 SSHFP output"
fi

if ${SSH} -Q key-plain | grep ssh-rsa >/dev/null; then
	fp=`${SSHKEYGEN} -r test -f ${SRC}/rsa_openssh.pub | awk '$5=="1"{print $6}'`
	if [ "$fp" != "99c79cc09f5f81069cc017cdf9552cfc94b3b929" ]; then
		fail "keygen fingerprint sha1"
	fi
	fp=`${SSHKEYGEN} -r test -f ${SRC}/rsa_openssh.pub | awk '$5=="2"{print $6}'`
	if [ "$fp" != \
	    "e30d6b9eb7a4de495324e4d5870b8220577993ea6af417e8e4a4f1c5bf01a9b6" ]; then
		fail "keygen fingerprint sha256"
	fi
fi

