#	$OpenBSD: principals-command.sh,v 1.14 2021/09/30 05:26:26 dtucker Exp $
#	Placed in the Public Domain.

tid="authorized principals command"

rm -f $OBJ/user_ca_key* $OBJ/cert_user_key*
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

if [ -z "$SUDO" -a ! -w /var/run ]; then
	skip "need SUDO to create file in /var/run, test won't work without"
fi

case "$SSH_KEYTYPES" in
	*ssh-rsa*)	userkeytype=rsa ;;
	*)		userkeytype=ed25519 ;;
esac

SERIAL=$$

# Create a CA key and a user certificate.
${SSHKEYGEN} -q -N '' -t ed25519  -f $OBJ/user_ca_key || \
	fatal "ssh-keygen of user_ca_key failed"
${SSHKEYGEN} -q -N '' -t ${userkeytype} -f $OBJ/cert_user_key || \
	fatal "ssh-keygen of cert_user_key failed"
${SSHKEYGEN} -q -s $OBJ/user_ca_key -I "Joanne User" \
    -z $$ -n ${USER},mekmitasdigoat $OBJ/cert_user_key || \
	fatal "couldn't sign cert_user_key"

CERT_BODY=`cat $OBJ/cert_user_key-cert.pub | awk '{ print $2 }'`
CA_BODY=`cat $OBJ/user_ca_key.pub | awk '{ print $2 }'`
CERT_FP=`${SSHKEYGEN} -lf $OBJ/cert_user_key-cert.pub | awk '{ print $2 }'`
CA_FP=`${SSHKEYGEN} -lf $OBJ/user_ca_key.pub | awk '{ print $2 }'`

# Establish a AuthorizedPrincipalsCommand in /var/run where it will have
# acceptable directory permissions.
PRINCIPALS_COMMAND="/var/run/principals_command_${LOGNAME}.$$"
trap "$SUDO rm -f ${PRINCIPALS_COMMAND}" 0
cat << _EOF | $SUDO sh -c "cat > '$PRINCIPALS_COMMAND'"
#!/bin/sh
test "x\$1" != "x${LOGNAME}" && exit 1
test "x\$2" != "xssh-${userkeytype}-cert-v01@openssh.com" && exit 1
test "x\$3" != "xssh-ed25519" && exit 1
test "x\$4" != "xJoanne User" && exit 1
test "x\$5" != "x${SERIAL}" && exit 1
test "x\$6" != "x${CA_FP}" && exit 1
test "x\$7" != "x${CERT_FP}" && exit 1
test "x\$8" != "x${CERT_BODY}" && exit 1
test "x\$9" != "x${CA_BODY}" && exit 1
test -f "$OBJ/authorized_principals_${LOGNAME}" &&
	exec cat "$OBJ/authorized_principals_${LOGNAME}"
_EOF
test $? -eq 0 || fatal "couldn't prepare principals command"
$SUDO chmod 0755 "$PRINCIPALS_COMMAND"

if ! $OBJ/check-perm -m keys-command $PRINCIPALS_COMMAND ; then
	echo "skipping: $PRINCIPALS_COMMAND is unsuitable as " \
	    "AuthorizedPrincipalsCommand"
	$SUDO rm -f $PRINCIPALS_COMMAND
	exit 0
fi

if [ ! -x $PRINCIPALS_COMMAND ]; then
	skip "$PRINCIPALS_COMMAND not executable " \
	    "(/var/run mounted noexec?)"
fi

# Test explicitly-specified principals
# Setup for AuthorizedPrincipalsCommand
rm -f $OBJ/authorized_keys_$USER
(
	cat $OBJ/sshd_proxy_bak
	echo "AuthorizedKeysFile none"
	echo "AuthorizedPrincipalsCommand $PRINCIPALS_COMMAND" \
	    "%u %t %T %i %s %F %f %k %K"
	echo "AuthorizedPrincipalsCommandUser ${LOGNAME}"
	echo "TrustedUserCAKeys $OBJ/user_ca_key.pub"
) > $OBJ/sshd_proxy

# XXX test missing command
# XXX test failing command

# Empty authorized_principals
verbose "$tid: empty authorized_principals"
echo > $OBJ/authorized_principals_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -eq 0 ]; then
	fail "ssh cert connect succeeded unexpectedly"
fi

# Wrong authorized_principals
verbose "$tid: wrong authorized_principals"
echo gregorsamsa > $OBJ/authorized_principals_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -eq 0 ]; then
	fail "ssh cert connect succeeded unexpectedly"
fi

# Correct authorized_principals
verbose "$tid: correct authorized_principals"
echo mekmitasdigoat > $OBJ/authorized_principals_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -ne 0 ]; then
	fail "ssh cert connect failed"
fi

# authorized_principals with bad key option
verbose "$tid: authorized_principals bad key opt"
echo 'blah mekmitasdigoat' > $OBJ/authorized_principals_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -eq 0 ]; then
	fail "ssh cert connect succeeded unexpectedly"
fi

# authorized_principals with command=false
verbose "$tid: authorized_principals command=false"
echo 'command="false" mekmitasdigoat' > \
    $OBJ/authorized_principals_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -eq 0 ]; then
	fail "ssh cert connect succeeded unexpectedly"
fi


# authorized_principals with command=true
verbose "$tid: authorized_principals command=true"
echo 'command="true" mekmitasdigoat' > \
    $OBJ/authorized_principals_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost false >/dev/null 2>&1
if [ $? -ne 0 ]; then
	fail "ssh cert connect failed"
fi

# Setup for principals= key option
# TODO: remove?
rm -f $OBJ/authorized_principals_$USER
(
	cat $OBJ/sshd_proxy_bak
) > $OBJ/sshd_proxy

# Wrong principals list
verbose "$tid: wrong principals key option"
(
	printf 'cert-authority,principals="gregorsamsa" '
	cat $OBJ/user_ca_key.pub
) > $OBJ/authorized_keys_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -eq 0 ]; then
	fail "ssh cert connect succeeded unexpectedly"
fi

# Correct principals list
verbose "$tid: correct principals key option"
(
	printf 'cert-authority,principals="mekmitasdigoat" '
	cat $OBJ/user_ca_key.pub
) > $OBJ/authorized_keys_$USER
${SSH} -i $OBJ/cert_user_key \
    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
if [ $? -ne 0 ]; then
	fail "ssh cert connect failed"
fi
