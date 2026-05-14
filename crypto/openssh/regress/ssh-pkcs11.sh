#	$OpenBSD: ssh-pkcs11.sh,v 1.1 2025/10/16 00:01:54 djm Exp $
#	Placed in the Public Domain.

tid="pkcs11 ssh test"

p11_setup || skip "No PKCS#11 library found"

grep -iv IdentityFile $OBJ/ssh_proxy |
	grep -vi BatchMode > $OBJ/ssh_proxy.orig
#echo "IdentitiesOnly=yes" >> $OBJ/ssh_proxy.orig
echo "PKCS11Provider=${TEST_SSH_PKCS11}" >> $OBJ/ssh_proxy.orig

check_all() {
	tag="$1"
	expect_success=$2
	pinsh="$3"
	for k in $ED25519 $RSA $EC; do
		kshort=`basename "$k"`
		verbose "$tag: $kshort"
		pub="$k.pub"
		cp $pub $OBJ/key.pub
		chmod 0600 $OBJ/key.pub
		cat $OBJ/key.pub > $OBJ/authorized_keys_$USER
		cp $OBJ/ssh_proxy.orig $OBJ/ssh_proxy
		env SSH_ASKPASS="$pinsh" SSH_ASKPASS_REQUIRE=force \
			${SSH} -F $OBJ/ssh_proxy somehost exit 5 >/dev/null 2>&1
		r=$?
		if [ "x$expect_success" = "xy" ]; then
			if [ $r -ne 5 ]; then
				fail "ssh connect failed (exit code $r)"
			fi
		elif [ $r -eq 5 ]; then
			fail "ssh connect succeeded unexpectedly (exit code $r)"
		fi
	done
}

check_all "correct pin" y $PIN_SH
check_all "wrong pin" n $WRONGPIN_SH
check_all "nopin" n `which true`
