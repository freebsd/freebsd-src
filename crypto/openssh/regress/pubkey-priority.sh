#	$OpenBSD: pubkey-priority.sh,v 1.2 2026/08/03 23:22:23 dtucker Exp $
#	Placed in the Public Domain.

tid="public key priority and ordering"

rm -f $OBJ/ca_key* $OBJ/explicit_agent* $OBJ/explicit_fs* \
	$OBJ/agent_sk_notouch* $OBJ/agent_sk_verify* $OBJ/agent_normal_cert* \
	$OBJ/agent_plain* $OBJ/id_ed25519*

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s` >/dev/null
r=$?
if [ $r -ne 0 ]; then
	fatal "could not start ssh-agent: exit code $r"
fi

# Generate CA
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/ca_key || fatal "ca keygen failed"

# 1. Explicit filesystem key
${SSHKEYGEN} -q -N '' -t ed25519 -C explicit_fs -f $OBJ/explicit_fs || fatal "keygen explicit_fs failed"

# 2. Explicit agent key
${SSHKEYGEN} -q -N '' -t ed25519 -C explicit_agent -f $OBJ/explicit_agent || fatal "keygen explicit_agent failed"

# 3. Implicit filesystem key (default id_ed25519)
${SSHKEYGEN} -q -N '' -t ed25519 -C implicit_fs -f $OBJ/id_ed25519 || fatal "keygen implicit failed"

# 4. Agent plain key
${SSHKEYGEN} -q -N '' -t ed25519 -C agent_plain -f $OBJ/agent_plain || fatal "keygen agent_plain failed"

# 5. Agent normal cert
${SSHKEYGEN} -q -N '' -t ed25519 -C agent_normal_cert -f $OBJ/agent_normal_cert || fatal "keygen agent_normal_cert failed"
${SSHKEYGEN} -qs $OBJ/ca_key -I "normal cert" -n estragon $OBJ/agent_normal_cert.pub || fatal "ca sign normal failed"

# 6. Agent SK verify-required cert
${SSHKEYGEN} -q -N '' -t ed25519-sk -C agent_sk_verify -f $OBJ/agent_sk_verify || fatal "keygen agent_sk_verify failed"
${SSHKEYGEN} -qs $OBJ/ca_key -I "verify cert" -n estragon -O verify-required $OBJ/agent_sk_verify.pub || fatal "ca sign verify failed"

# 7. Agent SK no-touch-required cert
${SSHKEYGEN} -q -N '' -t ed25519-sk -C agent_sk_notouch -f $OBJ/agent_sk_notouch || fatal "keygen agent_sk_notouch failed"
${SSHKEYGEN} -qs $OBJ/ca_key -I "notouch cert" -n estragon -O no-touch-required $OBJ/agent_sk_notouch.pub || fatal "ca sign notouch failed"

# Load agent keys in reverse order of expected priority
${SSHADD} $OBJ/agent_plain >/dev/null 2>&1 || fatal "ssh-add agent_plain failed"
${SSHADD} $OBJ/agent_normal_cert >/dev/null 2>&1 || fatal "ssh-add agent_normal_cert failed"
${SSHADD} $OBJ/agent_sk_verify >/dev/null 2>&1 || fatal "ssh-add agent_sk_verify failed"
${SSHADD} $OBJ/agent_sk_notouch >/dev/null 2>&1 || fatal "ssh-add agent_sk_notouch failed"
${SSHADD} $OBJ/explicit_agent >/dev/null 2>&1 || fatal "ssh-add explicit_agent failed"

cat > $OBJ/ssh_config_prio <<EOF
Host test
	HostName 127.0.0.1
	IdentityFile $OBJ/explicit_agent
	IdentityFile $OBJ/explicit_fs
EOF

trace "verify key priority order"
${SSH} -F $OBJ/ssh_config_prio -Z test > $OBJ/prio.out 2>&1 || fatal "ssh -Z failed"

cat > $OBJ/prio.expected <<EOF
$OBJ/explicit_agent ED25519
agent_sk_notouch ED25519-SK-CERT
agent_normal_cert ED25519-CERT
agent_sk_verify ED25519-SK-CERT
agent_plain ED25519
agent_normal_cert ED25519
agent_sk_verify ED25519-SK
agent_sk_notouch ED25519-SK
$OBJ/explicit_fs ED25519
EOF

awk '{print $1 " " $2}' < $OBJ/prio.out > $OBJ/prio.got
if ! cmp $OBJ/prio.expected $OBJ/prio.got >/dev/null 2>&1; then
	diff -u $OBJ/prio.expected $OBJ/prio.got
	fail "unexpected public key ordering"
fi

trace "verify IdentitiesOnly ordering"
${SSH} -F $OBJ/ssh_config_prio -oIdentitiesOnly=yes -Z test > $OBJ/prio_identities.out 2>&1 || fatal "ssh -Z IdentitiesOnly failed"

cat > $OBJ/prio_identities.expected <<EOF
$OBJ/explicit_agent ED25519
$OBJ/explicit_fs ED25519
EOF

awk '{print $1 " " $2}' < $OBJ/prio_identities.out > $OBJ/prio_identities.got
if ! cmp $OBJ/prio_identities.expected $OBJ/prio_identities.got >/dev/null 2>&1; then
	diff -u $OBJ/prio_identities.expected $OBJ/prio_identities.got
	fail "unexpected IdentitiesOnly public key ordering"
fi

trace "verify implicit key ordering"
HOME=$OBJ ${SSH} -F /dev/null -Z test > $OBJ/prio_implicit.out 2>&1 || fatal "ssh -Z implicit failed"

cat > $OBJ/prio_implicit.expected <<EOF
agent_sk_notouch ED25519-SK-CERT
agent_normal_cert ED25519-CERT
agent_sk_verify ED25519-SK-CERT
agent_plain ED25519
agent_normal_cert ED25519
agent_sk_verify ED25519-SK
agent_sk_notouch ED25519-SK
explicit_agent ED25519
$HOME/.ssh/id_rsa RSA
$HOME/.ssh/id_ecdsa ECDSA
$HOME/.ssh/id_ecdsa_sk ECDSA-SK
$HOME/.ssh/id_ed25519 ED25519
$HOME/.ssh/id_ed25519_sk 
$HOME/.ssh/id_mldsa44_ed25519 
EOF

awk '{print $1 " " $2}' <  $OBJ/prio_implicit.out > $OBJ/prio_implicit.got
if ! cmp $OBJ/prio_implicit.expected $OBJ/prio_implicit.got >/dev/null 2>&1; then
	diff -u $OBJ/prio_implicit.expected $OBJ/prio_implicit.got
	fail "unexpected implicit public key ordering"
fi

trace "kill agent"
${SSHAGENT} -k >/dev/null
