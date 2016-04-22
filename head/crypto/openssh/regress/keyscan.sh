#	$OpenBSD: keyscan.sh,v 1.5 2015/09/11 03:44:21 djm Exp $
#	Placed in the Public Domain.

tid="keyscan"

# remove DSA hostkey
rm -f ${OBJ}/host.dsa

start_sshd

KEYTYPES=`${SSH} -Q key-plain`
if ssh_version 1; then
	KEYTYPES="${KEYTYPES} rsa1"
fi

for t in $KEYTYPES; do
	trace "keyscan type $t"
	${SSHKEYSCAN} -t $t -p $PORT 127.0.0.1 127.0.0.1 127.0.0.1 \
		> /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-keyscan -t $t failed with: $r"
	fi
done
