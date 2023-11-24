#	$OpenBSD: hostkey-agent.sh,v 1.13 2021/09/30 05:20:08 dtucker Exp $
#	Placed in the Public Domain.

tid="hostkey agent"

rm -f $OBJ/agent-key.* $OBJ/ssh_proxy.orig $OBJ/known_hosts.orig $OBJ/agent-ca*

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s` > /dev/null
r=$?
[ $r -ne 0 ] && fatal "could not start ssh-agent: exit code $r"

grep -vi 'hostkey' $OBJ/sshd_proxy > $OBJ/sshd_proxy.orig
echo "HostKeyAgent $SSH_AUTH_SOCK" >> $OBJ/sshd_proxy.orig

trace "make CA key"

${SSHKEYGEN} -qt ed25519 -f $OBJ/agent-ca -N '' || fatal "ssh-keygen CA"

trace "load hostkeys"
for k in $SSH_KEYTYPES ; do
	${SSHKEYGEN} -qt $k -f $OBJ/agent-key.$k -N '' || fatal "ssh-keygen $k"
	${SSHKEYGEN} -s $OBJ/agent-ca -qh -n localhost-with-alias \
		-I localhost-with-alias $OBJ/agent-key.$k.pub || \
		fatal "sign $k"
	${SSHADD} -k $OBJ/agent-key.$k >/dev/null 2>&1 || \
		fatal "couldn't load key $OBJ/agent-key.$k"
	# Remove private key so the server can't use it.
	rm $OBJ/agent-key.$k || fatal "couldn't rm $OBJ/agent-key.$k"
done
rm $OBJ/agent-ca # Don't need CA private any more either

unset SSH_AUTH_SOCK

for k in $SSH_KEYTYPES ; do
	verbose "key type $k"
	cp $OBJ/sshd_proxy.orig $OBJ/sshd_proxy
	echo "HostKeyAlgorithms $k" >> $OBJ/sshd_proxy
	echo "Hostkey $OBJ/agent-key.${k}" >> $OBJ/sshd_proxy
	opts="-oHostKeyAlgorithms=$k -F $OBJ/ssh_proxy"
	( printf 'localhost-with-alias,127.0.0.1,::1 ' ;
	  cat $OBJ/agent-key.$k.pub) > $OBJ/known_hosts
	SSH_CONNECTION=`${SSH} $opts host 'echo $SSH_CONNECTION'`
	if [ $? -ne 0 ]; then
		fail "keytype $k failed"
	fi
	if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
		fail "bad SSH_CONNECTION key type $k"
	fi
done

SSH_CERTTYPES=`ssh -Q key-sig | grep 'cert-v01@openssh.com'`

# Prepare sshd_proxy for certificates.
cp $OBJ/sshd_proxy.orig $OBJ/sshd_proxy
HOSTKEYALGS=""
for k in $SSH_CERTTYPES ; do
	test -z "$HOSTKEYALGS" || HOSTKEYALGS="${HOSTKEYALGS},"
	HOSTKEYALGS="${HOSTKEYALGS}${k}"
done
for k in $SSH_KEYTYPES ; do
	echo "Hostkey $OBJ/agent-key.${k}.pub" >> $OBJ/sshd_proxy
	echo "HostCertificate $OBJ/agent-key.${k}-cert.pub" >> $OBJ/sshd_proxy
	test -f $OBJ/agent-key.${k}.pub || fatal "no $k key"
	test -f $OBJ/agent-key.${k}-cert.pub || fatal "no $k cert"
done
echo "HostKeyAlgorithms $HOSTKEYALGS" >> $OBJ/sshd_proxy

# Add only CA trust anchor to known_hosts.
( printf '@cert-authority localhost-with-alias ' ;
  cat $OBJ/agent-ca.pub) > $OBJ/known_hosts

for k in $SSH_CERTTYPES ; do
	verbose "cert type $k"
	opts="-oHostKeyAlgorithms=$k -F $OBJ/ssh_proxy"
	SSH_CONNECTION=`${SSH} $opts host 'echo $SSH_CONNECTION'`
	if [ $? -ne 0 ]; then
		fail "cert type $k failed"
	fi
	if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
		fail "bad SSH_CONNECTION key type $k"
	fi
done

trace "kill agent"
${SSHAGENT} -k > /dev/null

