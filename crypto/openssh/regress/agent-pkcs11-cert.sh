#	$OpenBSD: agent-pkcs11-cert.sh,v 1.3 2025/07/26 01:53:31 djm Exp $
#	Placed in the Public Domain.

tid="pkcs11 agent certificate test"

LC_ALL=C
export LC_ALL
p11_setup || skip "No PKCS#11 library found"

rm -f $OBJ/output_* $OBJ/expect_*
rm -f $OBJ/ca*

trace "generate CA key and certify keys"
$SSHKEYGEN -q -t ed25519 -C ca -N '' -f $OBJ/ca ||  fatal "ssh-keygen CA failed"
$SSHKEYGEN -qs $OBJ/ca -I "ecdsa_key" -n $USER -z 1 ${SSH_SOFTHSM_DIR}/EC.pub ||
	fatal "certify ECDSA key failed"
$SSHKEYGEN -qs $OBJ/ca -I "rsa_key" -n $USER -z 2 ${SSH_SOFTHSM_DIR}/RSA.pub ||
	fatal "certify RSA key failed"
$SSHKEYGEN -qs $OBJ/ca -I "ed25519_key" -n $USER -z 3 \
    ${SSH_SOFTHSM_DIR}/ED25519.pub ||
	fatal "certify ed25519 key failed"
$SSHKEYGEN -qs $OBJ/ca -I "ca_ca" -n $USER -z 4 $OBJ/ca.pub ||
	fatal "certify CA key failed"

start_ssh_agent

verbose "load pkcs11 keys and certs"
# Note: deliberately contains non-cert keys and non-matching cert on commandline
p11_ssh_add -qs ${TEST_SSH_PKCS11} \
    $OBJ/ca.pub \
    ${SSH_SOFTHSM_DIR}/ED25519.pub \
    ${SSH_SOFTHSM_DIR}/ED25519-cert.pub \
    ${SSH_SOFTHSM_DIR}/EC.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub ||
	fatal "failed to add keys"
# Verify their presence
verbose "verify presence"
cut -d' ' -f1-2 \
    ${SSH_SOFTHSM_DIR}/ED25519.pub \
    ${SSH_SOFTHSM_DIR}/EC.pub \
    ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/ED25519-cert.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub | sort > $OBJ/expect_list
$SSHADD -L | cut -d' ' -f1-2 | sort > $OBJ/output_list
diff $OBJ/expect_list $OBJ/output_list

# Verify that all can perform signatures.
verbose "check signatures"
for x in ${SSH_SOFTHSM_DIR}/EC.pub ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub ${SSH_SOFTHSM_DIR}/RSA-cert.pub \
    ${SSH_SOFTHSM_DIR}/ED25519.pub ${SSH_SOFTHSM_DIR}/ED25519-cert.pub ; do
	$SSHADD -T $x || fail "Signing failed for $x"
done

# Delete plain keys.
verbose "delete plain keys"
$SSHADD -qd ${SSH_SOFTHSM_DIR}/EC.pub ${SSH_SOFTHSM_DIR}/RSA.pub
$SSHADD -qd ${SSH_SOFTHSM_DIR}/ED25519.pub 
# Verify that certs can still perform signatures.
verbose "reverify certificate signatures"
for x in ${SSH_SOFTHSM_DIR}/EC-cert.pub ${SSH_SOFTHSM_DIR}/RSA-cert.pub \
    ${SSH_SOFTHSM_DIR}/ED25519-cert.pub ; do
	$SSHADD -T $x || fail "Signing failed for $x"
done

$SSHADD -qD >/dev/null || fatal "clear agent failed"

verbose "load pkcs11 certs only"
p11_ssh_add -qCs ${TEST_SSH_PKCS11} \
    $OBJ/ca.pub \
    ${SSH_SOFTHSM_DIR}/EC.pub \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub \
    ${SSH_SOFTHSM_DIR}/ED25519.pub \
    ${SSH_SOFTHSM_DIR}/ED25519-cert.pub ||
	fatal "failed to add keys"
# Verify their presence
verbose "verify presence"
cut -d' ' -f1-2 \
    ${SSH_SOFTHSM_DIR}/EC-cert.pub \
    ${SSH_SOFTHSM_DIR}/RSA-cert.pub \
    ${SSH_SOFTHSM_DIR}/ED25519-cert.pub | sort > $OBJ/expect_list
$SSHADD -L | cut -d' ' -f1-2 | sort > $OBJ/output_list
diff $OBJ/expect_list $OBJ/output_list

# Verify that certs can perform signatures.
verbose "check signatures"
for x in ${SSH_SOFTHSM_DIR}/EC-cert.pub ${SSH_SOFTHSM_DIR}/RSA-cert.pub \
    ${SSH_SOFTHSM_DIR}/ED25519-cert.pub ; do
	$SSHADD -T $x || fail "Signing failed for $x"
done
