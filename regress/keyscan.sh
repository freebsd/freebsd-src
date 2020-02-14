#	$OpenBSD: keyscan.sh,v 1.9 2019/01/28 03:50:39 dtucker Exp $
#	Placed in the Public Domain.

tid="keyscan"

KEYTYPES=`${SSH} -Q key-plain`
for i in $KEYTYPES; do
	if [ -z "$algs" ]; then
		algs="$i"
	else
		algs="$algs,$i"
	fi
done
echo "HostKeyAlgorithms $algs" >> $OBJ/sshd_config

start_sshd

for t in $KEYTYPES; do
	trace "keyscan type $t"
	${SSHKEYSCAN} -t $t -p $PORT 127.0.0.1 127.0.0.1 127.0.0.1 \
		> /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-keyscan -t $t failed with: $r"
	fi
done
