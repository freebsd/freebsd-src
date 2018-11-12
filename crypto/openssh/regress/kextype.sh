#	$OpenBSD: kextype.sh,v 1.4 2013/11/07 04:26:56 dtucker Exp $
#	Placed in the Public Domain.

tid="login with different key exchange algorithms"

TIME=/usr/bin/time
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak
cp $OBJ/ssh_proxy $OBJ/ssh_proxy_bak

tries="1 2 3 4"
for k in `${SSH} -Q kex`; do
	verbose "kex $k"
	for i in $tries; do
		${SSH} -F $OBJ/ssh_proxy -o KexAlgorithms=$k x true
		if [ $? -ne 0 ]; then
			fail "ssh kex $k"
		fi
	done
done

