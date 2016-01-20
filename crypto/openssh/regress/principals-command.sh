#	$OpenBSD: principals-command.sh,v 1.1 2015/05/21 06:44:25 djm Exp $
#	Placed in the Public Domain.

tid="authorized principals command"

rm -f $OBJ/user_ca_key* $OBJ/cert_user_key*
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

if test -z "$SUDO" ; then
	echo "skipped (SUDO not set)"
	echo "need SUDO to create file in /var/run, test won't work without"
	exit 0
fi

# Establish a AuthorizedPrincipalsCommand in /var/run where it will have
# acceptable directory permissions.
PRINCIPALS_COMMAND="/var/run/principals_command_${LOGNAME}"
cat << _EOF | $SUDO sh -c "cat > '$PRINCIPALS_COMMAND'"
#!/bin/sh
test "x\$1" != "x${LOGNAME}" && exit 1
test -f "$OBJ/authorized_principals_${LOGNAME}" &&
	exec cat "$OBJ/authorized_principals_${LOGNAME}"
_EOF
test $? -eq 0 || fatal "couldn't prepare principals command"
$SUDO chmod 0755 "$PRINCIPALS_COMMAND"

# Create a CA key and a user certificate.
${SSHKEYGEN} -q -N '' -t ed25519  -f $OBJ/user_ca_key || \
	fatal "ssh-keygen of user_ca_key failed"
${SSHKEYGEN} -q -N '' -t ed25519 -f $OBJ/cert_user_key || \
	fatal "ssh-keygen of cert_user_key failed"
${SSHKEYGEN} -q -s $OBJ/user_ca_key -I "regress user key for $USER" \
    -z $$ -n ${USER},mekmitasdigoat $OBJ/cert_user_key || \
	fatal "couldn't sign cert_user_key"

# Test explicitly-specified principals
for privsep in yes no ; do
	_prefix="privsep $privsep"

	# Setup for AuthorizedPrincipalsCommand
	rm -f $OBJ/authorized_keys_$USER
	(
		cat $OBJ/sshd_proxy_bak
		echo "UsePrivilegeSeparation $privsep"
		echo "AuthorizedKeysFile none"
		echo "AuthorizedPrincipalsCommand $PRINCIPALS_COMMAND %u"
		echo "AuthorizedPrincipalsCommandUser ${LOGNAME}"
		echo "TrustedUserCAKeys $OBJ/user_ca_key.pub"
	) > $OBJ/sshd_proxy

	# XXX test missing command
	# XXX test failing command

	# Empty authorized_principals
	verbose "$tid: ${_prefix} empty authorized_principals"
	echo > $OBJ/authorized_principals_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect succeeded unexpectedly"
	fi

	# Wrong authorized_principals
	verbose "$tid: ${_prefix} wrong authorized_principals"
	echo gregorsamsa > $OBJ/authorized_principals_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect succeeded unexpectedly"
	fi

	# Correct authorized_principals
	verbose "$tid: ${_prefix} correct authorized_principals"
	echo mekmitasdigoat > $OBJ/authorized_principals_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		fail "ssh cert connect failed"
	fi

	# authorized_principals with bad key option
	verbose "$tid: ${_prefix} authorized_principals bad key opt"
	echo 'blah mekmitasdigoat' > $OBJ/authorized_principals_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect succeeded unexpectedly"
	fi

	# authorized_principals with command=false
	verbose "$tid: ${_prefix} authorized_principals command=false"
	echo 'command="false" mekmitasdigoat' > \
	    $OBJ/authorized_principals_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect succeeded unexpectedly"
	fi


	# authorized_principals with command=true
	verbose "$tid: ${_prefix} authorized_principals command=true"
	echo 'command="true" mekmitasdigoat' > \
	    $OBJ/authorized_principals_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost false >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		fail "ssh cert connect failed"
	fi

	# Setup for principals= key option
	rm -f $OBJ/authorized_principals_$USER
	(
		cat $OBJ/sshd_proxy_bak
		echo "UsePrivilegeSeparation $privsep"
	) > $OBJ/sshd_proxy

	# Wrong principals list
	verbose "$tid: ${_prefix} wrong principals key option"
	(
		printf 'cert-authority,principals="gregorsamsa" '
		cat $OBJ/user_ca_key.pub
	) > $OBJ/authorized_keys_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect succeeded unexpectedly"
	fi

	# Correct principals list
	verbose "$tid: ${_prefix} correct principals key option"
	(
		printf 'cert-authority,principals="mekmitasdigoat" '
		cat $OBJ/user_ca_key.pub
	) > $OBJ/authorized_keys_$USER
	${SSH} -2i $OBJ/cert_user_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		fail "ssh cert connect failed"
	fi
done
