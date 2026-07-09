#	$OpenBSD: keyscan.sh,v 1.14 2026/04/27 05:49:41 dtucker Exp $
#	Placed in the Public Domain.

tid="keyscan"

# Enable all supported host key algos.
algs=""
for i in `$SSH -Q HostKeyAlgorithms`; do
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
