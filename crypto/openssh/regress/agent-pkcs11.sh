#	$OpenBSD: agent-pkcs11.sh,v 1.15 2025/07/26 01:53:31 djm Exp $
#	Placed in the Public Domain.

tid="pkcs11 agent test"

p11_setup || skip "No PKCS#11 library found"

start_ssh_agent

trace "add pkcs11 key to agent"
p11_ssh_add -s ${TEST_SSH_PKCS11} > /dev/null 2>&1
r=$?
if [ $r -ne 0 ]; then
	fail "ssh-add -s failed: exit code $r"
fi

trace "pkcs11 list via agent"
${SSHADD} -l > /dev/null 2>&1
r=$?
if [ $r -ne 0 ]; then
	fail "ssh-add -l failed: exit code $r"
fi

for k in $ED25519 $RSA $EC; do
	trace "testing $k"
	pub=$(cat $k.pub)
	${SSHADD} -L | grep -q "$pub" || \
		fail "key $k missing in ssh-add -L"
	${SSHADD} -T $k.pub || fail "ssh-add -T with $k failed"

	# add to authorized keys
	cat $k.pub > $OBJ/authorized_keys_$USER
	trace "pkcs11 connect via agent ($k)"
	${SSH} -F $OBJ/ssh_proxy somehost exit 5
	r=$?
	if [ $r -ne 5 ]; then
		fail "ssh connect failed (exit code $r)"
	fi
done

trace "remove pkcs11 keys"
p11_ssh_add -e ${TEST_SSH_PKCS11} > /dev/null 2>&1
r=$?
if [ $r -ne 0 ]; then
	fail "ssh-add -e failed: exit code $r"
fi
