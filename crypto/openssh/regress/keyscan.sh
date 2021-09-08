#	$OpenBSD: keyscan.sh,v 1.13 2020/01/22 07:31:27 dtucker Exp $
#	Placed in the Public Domain.

tid="keyscan"

for i in $SSH_KEYTYPES; do
	if [ -z "$algs" ]; then
		algs="$i"
	else
		algs="$algs,$i"
	fi
done
echo "HostKeyAlgorithms $algs" >> $OBJ/sshd_config

start_sshd

for t in $SSH_KEYTYPES; do
	trace "keyscan type $t"
	${SSHKEYSCAN} -t $t -T 15 -p $PORT 127.0.0.1 127.0.0.1 127.0.0.1 \
		> /dev/null 2>&1
	r=$?
	if [ $r -ne 0 ]; then
		fail "ssh-keyscan -t $t failed with: $r"
	fi
done
