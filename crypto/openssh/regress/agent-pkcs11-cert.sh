#	$OpenBSD: agent-pkcs11-cert.sh,v 1.1 2023/12/18 14:50:08 djm Exp $
#	Placed in the Public Domain.

tid="pkcs11 agent certificate test"

SSH_AUTH_SOCK="$OBJ/agent.sock"
export SSH_AUTH_SOCK
LC_ALL=C
export LC_ALL
p11_setup || skip "No PKCS#11 library found"

rm -f $SSH_AUTH_SOCK $OBJ/agent.log
rm -f $OBJ/output_* $OBJ/expect_*
rm -f $OBJ/ca*

trace "generate CA key and certify keys"
$SSHKEYGEN -q -t ed25519 -C ca -N '' -f $OBJ/ca ||  fatal "ssh-keygen CA failed"
$SSHKEYGEN -qs $OBJ/ca -I "ecdsa_key" -n $USER -z 1 ${SSH_SOFTHSM_DIR}/EC.pub ||
	fatal "certify ECDSA key failed"
$SSHKEYGEN -qs $OBJ/ca -I "rsa_key" -n $USER -z 2 ${SSH_SOFTHSM_DIR}/RSA.pub ||
	fatal "certify RSA key failed"
$SSHKEYGEN -qs $OBJ/ca -I "ca_ca" -n $USER -z 3 $OBJ/ca.pub ||
	fatal "certify CA key failed"

rm -f $SSH_AUTH_SOCK
trace "start agent"
${SSHAGENT} ${EXTRA_AGENT_ARGS} -d -a $SSH_AUTH_SOCK > $OBJ/agent.log 2>&1 &
AGENT_PID=$!
trap "kill $AGENT_PID" EXIT
for x in 0 1 2 3 4 ; do
	# Give it a chance to start
	${SSHADD} -l > /dev/null 2>&1
	r=$?
	test $r -eq 1 && break
	sleep 1
done
if [ $r -ne 1 ]; then
	fatal "ssh-add -l did not fail with exit code 1 (got $r)"
fi

trace "load pkcs11 keys and certs"
# Note: deliberately contains non-cert keys and non-matching cert on commandline
p11_ssh_add -qs ${TEST_SSH_PKCS11} \
    $OBJ/ca.pub \
    ${SSH_SOFTHSM_DIR}/EC.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub ||
	fatal "failed to add keys"
# Verify their presence
cut -d' ' -f1-2 \
    ${SSH_SOFTHSM_DIR}/EC.pub \
    ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub | sort > $OBJ/expect_list
$SSHADD -L | cut -d' ' -f1-2 | sort > $OBJ/output_list
diff $OBJ/expect_list $OBJ/output_list

# Verify that all can perform signatures.
for x in ${SSH_SOFTHSM_DIR}/EC.pub ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub ${SSH_SOFTHSM_DIR}/RSA-cert.pub ; do
	$SSHADD -T $x || fail "Signing failed for $x"
done

# Delete plain keys.
$SSHADD -qd ${SSH_SOFTHSM_DIR}/EC.pub ${SSH_SOFTHSM_DIR}/RSA.pub
# Verify that certs can still perform signatures.
for x in ${SSH_SOFTHSM_DIR}/EC-cert.pub ${SSH_SOFTHSM_DIR}/RSA-cert.pub ; do
	$SSHADD -T $x || fail "Signing failed for $x"
done

$SSHADD -qD >/dev/null || fatal "clear agent failed"

trace "load pkcs11 certs only"
p11_ssh_add -qCs ${TEST_SSH_PKCS11} \
    $OBJ/ca.pub \
    ${SSH_SOFTHSM_DIR}/EC.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub ||
	fatal "failed to add keys"
# Verify their presence
cut -d' ' -f1-2 \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub | sort > $OBJ/expect_list
$SSHADD -L | cut -d' ' -f1-2 | sort > $OBJ/output_list
diff $OBJ/expect_list $OBJ/output_list

# Verify that certs can perform signatures.
for x in ${SSH_SOFTHSM_DIR}/EC-cert.pub ${SSH_SOFTHSM_DIR}/RSA-cert.pub ; do
	$SSHADD -T $x || fail "Signing failed for $x"
done
