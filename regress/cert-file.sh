#	$OpenBSD: cert-file.sh,v 1.2 2015/09/24 07:15:39 djm Exp $
#	Placed in the Public Domain.

tid="ssh with certificates"

rm -f $OBJ/user_ca_key* $OBJ/user_key*
rm -f $OBJ/cert_user_key*

# Create a CA key
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/user_ca_key1 ||\
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t ed25519  -f $OBJ/user_ca_key2 ||\
	fatal "ssh-keygen failed"

# Make some keys and certificates.
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/user_key1 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/user_key2 || \
	fatal "ssh-keygen failed"
# Move the certificate to a different address to better control
# when it is offered.
${SSHKEYGEN} -q -s $OBJ/user_ca_key1 -I "regress user key for $USER" \
	-z $$ -n ${USER} $OBJ/user_key1 ||
		fail "couldn't sign user_key1 with user_ca_key1"
mv $OBJ/user_key1-cert.pub $OBJ/cert_user_key1_1.pub
${SSHKEYGEN} -q -s $OBJ/user_ca_key2 -I "regress user key for $USER" \
	-z $$ -n ${USER} $OBJ/user_key1 ||
		fail "couldn't sign user_key1 with user_ca_key2"
mv $OBJ/user_key1-cert.pub $OBJ/cert_user_key1_2.pub

trace 'try with identity files'
opts="-F $OBJ/ssh_proxy -oIdentitiesOnly=yes"
opts2="$opts -i $OBJ/user_key1 -i $OBJ/user_key2"
echo "cert-authority $(cat $OBJ/user_ca_key1.pub)" > $OBJ/authorized_keys_$USER

for p in ${SSH_PROTOCOLS}; do
	# Just keys should fail
	${SSH} $opts2 somehost exit 5$p
	r=$?
	if [ $r -eq 5$p ]; then
		fail "ssh succeeded with no certs in protocol $p"
	fi

	# Keys with untrusted cert should fail.
	opts3="$opts2 -oCertificateFile=$OBJ/cert_user_key1_2.pub"
	${SSH} $opts3 somehost exit 5$p
	r=$?
	if [ $r -eq 5$p ]; then
		fail "ssh succeeded with bad cert in protocol $p"
	fi

	# Good cert with bad key should fail.
	opts3="$opts -i $OBJ/user_key2"
	opts3="$opts3 -oCertificateFile=$OBJ/cert_user_key1_1.pub"
	${SSH} $opts3 somehost exit 5$p
	r=$?
	if [ $r -eq 5$p ]; then
		fail "ssh succeeded with no matching key in protocol $p"
	fi

	# Keys with one trusted cert, should succeed.
	opts3="$opts2 -oCertificateFile=$OBJ/cert_user_key1_1.pub"
	${SSH} $opts3 somehost exit 5$p
	r=$?
	if [ $r -ne 5$p ]; then
		fail "ssh failed with trusted cert and key in protocol $p"
	fi

	# Multiple certs and keys, with one trusted cert, should succeed.
	opts3="$opts2 -oCertificateFile=$OBJ/cert_user_key1_2.pub"
	opts3="$opts3 -oCertificateFile=$OBJ/cert_user_key1_1.pub"
	${SSH} $opts3 somehost exit 5$p
	r=$?
	if [ $r -ne 5$p ]; then
		fail "ssh failed with multiple certs in protocol $p"
	fi

	#Keys with trusted certificate specified in config options, should succeed.
	opts3="$opts2 -oCertificateFile=$OBJ/cert_user_key1_1.pub"
	${SSH} $opts3 somehost exit 5$p
	r=$?
	if [ $r -ne 5$p ]; then
		fail "ssh failed with trusted cert in config in protocol $p"
	fi
done

#next, using an agent in combination with the keys
SSH_AUTH_SOCK=/nonexistent ${SSHADD} -l > /dev/null 2>&1
if [ $? -ne 2 ]; then
	fatal "ssh-add -l did not fail with exit code 2"
fi

trace "start agent"
eval `${SSHAGENT} -s` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fatal "could not start ssh-agent: exit code $r"
fi

# add private keys to agent
${SSHADD} -k $OBJ/user_key2 > /dev/null 2>&1
if [ $? -ne 0 ]; then
	fatal "ssh-add did not succeed with exit code 0"
fi
${SSHADD} -k $OBJ/user_key1 > /dev/null 2>&1
if [ $? -ne 0 ]; then
	fatal "ssh-add did not succeed with exit code 0"
fi

# try ssh with the agent and certificates
# note: ssh agent only uses certificates in protocol 2
opts="-F $OBJ/ssh_proxy"
# with no certificates, shoud fail
${SSH} -2 $opts somehost exit 52
if [ $? -eq 52 ]; then
	fail "ssh connect with agent in protocol 2 succeeded with no cert"
fi

#with an untrusted certificate, should fail
opts="$opts -oCertificateFile=$OBJ/cert_user_key1_2.pub"
${SSH} -2 $opts somehost exit 52
if [ $? -eq 52 ]; then
	fail "ssh connect with agent in protocol 2 succeeded with bad cert"
fi

#with an additional trusted certificate, should succeed
opts="$opts -oCertificateFile=$OBJ/cert_user_key1_1.pub"
${SSH} -2 $opts somehost exit 52
if [ $? -ne 52 ]; then
	fail "ssh connect with agent in protocol 2 failed with good cert"
fi

trace "kill agent"
${SSHAGENT} -k > /dev/null

#cleanup
rm -f $OBJ/user_ca_key* $OBJ/user_key*
rm -f $OBJ/cert_user_key*
