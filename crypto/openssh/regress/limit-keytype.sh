#	$OpenBSD: limit-keytype.sh,v 1.10 2021/02/25 03:27:34 djm Exp $
#	Placed in the Public Domain.

tid="restrict pubkey type"

# XXX sk-* keys aren't actually tested ATM.

rm -f $OBJ/authorized_keys_$USER $OBJ/user_ca_key* $OBJ/user_key*
rm -f $OBJ/authorized_principals_$USER $OBJ/cert_user_key*

mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
mv $OBJ/ssh_proxy $OBJ/ssh_proxy.orig

ktype1=ed25519; ktype2=ed25519; ktype3=ed25519;
ktype4=ed25519; ktype5=ed25519; ktype6=ed25519;
for t in $SSH_KEYTYPES ; do 
	case "$t" in
		ssh-rsa)	ktype2=rsa ;;
		ecdsa*)		ktype3=ecdsa ;;  # unused
		ssh-dss)	ktype4=dsa ;;
		sk-ssh-ed25519@openssh.com)		ktype5=ed25519-sk ;;
		sk-ecdsa-sha2-nistp256@openssh.com)	ktype6=ecdsa-sk ;;
	esac
done

# Create a CA key
${SSHKEYGEN} -q -N '' -t $ktype1 -f $OBJ/user_ca_key ||\
	fatal "ssh-keygen failed"

# Make some keys and a certificate.
${SSHKEYGEN} -q -N '' -t $ktype1 -f $OBJ/user_key1 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t $ktype2 -f $OBJ/user_key2 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t $ktype2 -f $OBJ/user_key3 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t $ktype4 -f $OBJ/user_key4 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t $ktype5 -f $OBJ/user_key5 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -N '' -t $ktype6 -f $OBJ/user_key6 || \
	fatal "ssh-keygen failed"
${SSHKEYGEN} -q -s $OBJ/user_ca_key -I "regress user key for $USER" \
	-z $$ -n ${USER},mekmitasdigoat $OBJ/user_key3 ||
		fatal "couldn't sign user_key1"
# Copy the private key alongside the cert to allow better control of when
# it is offered.
mv $OBJ/user_key3-cert.pub $OBJ/cert_user_key3.pub

grep -v IdentityFile $OBJ/ssh_proxy.orig > $OBJ/ssh_proxy

opts="-oProtocol=2 -F $OBJ/ssh_proxy -oIdentitiesOnly=yes"
certopts="$opts -i $OBJ/user_key3 -oCertificateFile=$OBJ/cert_user_key3.pub"

echo mekmitasdigoat > $OBJ/authorized_principals_$USER
cat $OBJ/user_key1.pub > $OBJ/authorized_keys_$USER
cat $OBJ/user_key2.pub >> $OBJ/authorized_keys_$USER

prepare_config() {
	(
		grep -v "Protocol"  $OBJ/sshd_proxy.orig
		echo "Protocol 2"
		echo "AuthenticationMethods publickey"
		echo "TrustedUserCAKeys $OBJ/user_ca_key.pub"
		echo "AuthorizedPrincipalsFile $OBJ/authorized_principals_%u"
		for x in "$@" ; do
			echo "$x"
		done
 	) > $OBJ/sshd_proxy
}

# Return the required parameter for PubkeyAcceptedAlgorithms corresponding to
# the supplied key type.
keytype() {
	case "$1" in
		ecdsa)		printf "ecdsa-sha2-*" ;;
		ed25519)	printf "ssh-ed25519" ;;
		dsa)		printf "ssh-dss" ;;
		rsa)		printf "rsa-sha2-256,rsa-sha2-512,ssh-rsa" ;;
		sk-ecdsa)	printf "sk-ecdsa-*" ;;
		sk-ssh-ed25519)	printf "sk-ssh-ed25519-*" ;;
	esac
}

prepare_config

# Check we can log in with all key types.
${SSH} $certopts proxy true || fatal "cert failed"
${SSH} $opts -i $OBJ/user_key1 proxy true || fatal "key1 failed"
${SSH} $opts -i $OBJ/user_key2 proxy true || fatal "key2 failed"

# Allow plain Ed25519 and RSA. The certificate should fail.
verbose "allow $ktype2,$ktype1"
prepare_config \
	"PubkeyAcceptedAlgorithms `keytype $ktype2`,`keytype $ktype1`"
${SSH} $certopts proxy true && fatal "cert succeeded"
${SSH} $opts -i $OBJ/user_key1 proxy true || fatal "key1 failed"
${SSH} $opts -i $OBJ/user_key2 proxy true || fatal "key2 failed"

# Allow Ed25519 only.
verbose "allow $ktype1"
prepare_config "PubkeyAcceptedAlgorithms `keytype $ktype1`"
${SSH} $certopts proxy true && fatal "cert succeeded"
${SSH} $opts -i $OBJ/user_key1 proxy true || fatal "key1 failed"
if [ "$ktype1" != "$ktype2" ]; then
	${SSH} $opts -i $OBJ/user_key2 proxy true && fatal "key2 succeeded"
fi

# Allow all certs. Plain keys should fail.
verbose "allow cert only"
prepare_config "PubkeyAcceptedAlgorithms *-cert-v01@openssh.com"
${SSH} $certopts proxy true || fatal "cert failed"
${SSH} $opts -i $OBJ/user_key1 proxy true && fatal "key1 succeeded"
${SSH} $opts -i $OBJ/user_key2 proxy true && fatal "key2 succeeded"

# Allow RSA in main config, Ed25519 for non-existent user.
verbose "match w/ no match"
prepare_config "PubkeyAcceptedAlgorithms `keytype $ktype2`" \
	"Match user x$USER" "PubkeyAcceptedAlgorithms +`keytype $ktype1`"
${SSH} $certopts proxy true && fatal "cert succeeded"
if [ "$ktype1" != "$ktype2" ]; then
	${SSH} $opts -i $OBJ/user_key1 proxy true && fatal "key1 succeeded"
fi
${SSH} $opts -i $OBJ/user_key2 proxy true || fatal "key2 failed"

# Allow only DSA in main config, Ed25519 for user.
verbose "match w/ matching"
prepare_config "PubkeyAcceptedAlgorithms `keytype $ktype4`" \
	"Match user $USER" "PubkeyAcceptedAlgorithms +`keytype $ktype1`"
${SSH} $certopts proxy true || fatal "cert failed"
${SSH} $opts -i $OBJ/user_key1 proxy true || fatal "key1 failed"
${SSH} $opts -i $OBJ/user_key4 proxy true && fatal "key4 succeeded"

