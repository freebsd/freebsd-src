#	$OpenBSD: keyscan.sh,v 1.15 2026/08/10 23:28:15 djm Exp $
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

trace "keyscan timeout for partial pre-KEX banner"
keyscan_port=`expr $PORT + 1`
keyscan_server_log=$OBJ/keyscan-server.log
(printf x; sleep 5) | \
    $NC -l 127.0.0.1 $keyscan_port >$keyscan_server_log 2>&1 &
keyscan_server_pid=$!
sleep 1
if ! kill -0 $keyscan_server_pid >/dev/null 2>&1; then
	wait $keyscan_server_pid
	fatal "keyscan test server failed to start"
fi
${SSHKEYSCAN} -t ed25519 -T 1 -p $keyscan_port 127.0.0.1 \
    >/dev/null 2>&1 &
keyscan_pid=$!
i=0
while kill -0 $keyscan_pid >/dev/null 2>&1 && test $i -lt 10; do
	sleep 1
	i=`expr $i + 1`
done
if kill -0 $keyscan_pid >/dev/null 2>&1; then
	kill $keyscan_pid >/dev/null 2>&1
	wait $keyscan_pid
	r=0
else
	wait $keyscan_pid
	r=$?
fi
kill $keyscan_server_pid >/dev/null 2>&1
wait $keyscan_server_pid
if ! grep "OpenSSH-keyscan" $keyscan_server_log >/dev/null 2>&1; then
	fail "ssh-keyscan did not connect to partial banner server"
elif test $r -eq 0; then
	fail "ssh-keyscan did not enforce timeout for partial banner"
elif test $r -ne 1; then
	fail "ssh-keyscan partial banner returned unexpected status: $r"
fi
