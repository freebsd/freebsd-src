#	$OpenBSD: multipubkey.sh,v 1.4 2021/06/07 01:16:34 djm Exp $
#	Placed in the Public Domain.

tid="multiple pubkey"

rm -f $OBJ/authorized_keys_$USER $OBJ/user_ca_key* $OBJ/user_key*
rm -f $OBJ/authorized_principals_$USER $OBJ/cert_user_key*

mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
mv $OBJ/ssh_proxy $OBJ/ssh_proxy.orig

# Create a CA key
${SSHKEYGEN} -q -N '' -t ed25519  -f $OBJ/user_ca_key ||\
	fatal "ssh-keygen failed"

# Make some keys and a certificate.
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/user_key1 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/user_key2 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -s $OBJ/user_ca_key -I "regress user key for $USER" \
	-z $$ -n ${USER},mekmitasdigoat $OBJ/user_key1 ||
		fail "couldn't sign user_key1"
# Copy the private key alongside the cert to allow better control of when
# it is offered.
mv $OBJ/user_key1-cert.pub $OBJ/cert_user_key1.pub
cp -p $OBJ/user_key1 $OBJ/cert_user_key1

grep -v IdentityFile $OBJ/ssh_proxy.orig > $OBJ/ssh_proxy

opts="-oProtocol=2 -F $OBJ/ssh_proxy -oIdentitiesOnly=yes"
opts="$opts -i $OBJ/cert_user_key1 -i $OBJ/user_key1 -i $OBJ/user_key2"

for match in no yes ; do
	(
		cat  $OBJ/sshd_proxy.orig
		echo "Protocol 2"
		echo "TrustedUserCAKeys $OBJ/user_ca_key.pub"
		echo "AuthorizedPrincipalsFile $OBJ/authorized_principals_%u"
 	) > $OBJ/sshd_proxy
	if test "$match" = "yes" ; then
		echo "AuthenticationMethods none" >> $OBJ/sshd_proxy
		echo "PubkeyAuthentication no" >> $OBJ/sshd_proxy
		echo "Match all" >> $OBJ/sshd_proxy
		echo "PubkeyAuthentication yes" >> $OBJ/sshd_proxy
	fi
	echo "AuthenticationMethods publickey,publickey" >> $OBJ/sshd_proxy

	# Single key should fail.
	trace "match $match single key"
	rm -f $OBJ/authorized_principals_$USER
	cat $OBJ/user_key1.pub > $OBJ/authorized_keys_$USER
	${SSH} $opts proxy true && fail "ssh succeeded with key"

	# Single key with same-public cert should fail.
	trace "match $match pubkey + identical cert"
	echo mekmitasdigoat > $OBJ/authorized_principals_$USER
	cat $OBJ/user_key1.pub > $OBJ/authorized_keys_$USER
	${SSH} $opts proxy true && fail "ssh succeeded with key+cert"

	# Multiple plain keys should succeed.
	trace "match $match multiple public"
	rm -f $OBJ/authorized_principals_$USER
	cat $OBJ/user_key1.pub $OBJ/user_key2.pub > \
	    $OBJ/authorized_keys_$USER
	${SSH} $opts proxy true || fail "ssh failed with multiple keys"
	# Cert and different key should succeed

	# Key and different-public cert should succeed.
	trace "match $match pubkey + different cert"
	echo mekmitasdigoat > $OBJ/authorized_principals_$USER
	cat $OBJ/user_key2.pub > $OBJ/authorized_keys_$USER
	${SSH} $opts proxy true || fail "ssh failed with key/cert"
done

