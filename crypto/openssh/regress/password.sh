#	$OpenBSD: password.sh,v 1.2 2025/06/29 08:20:21 dtucker Exp $
#	Placed in the Public Domain.
#
# This tests standard "password" authentication.  It does not run by default,
# and needs to be enabled by putting the password of the user running the tests
# into ${OBJ}/password.  Since this obviously puts the password at risk it is
# recommended to do this on a throwaway VM by setting a random password
# (and randomizing it again after the test, if you can't immediately dispose
# of the VM).

tid="password"

if [ -z "$SUDO" -o ! -f ${OBJ}/password ]; then
	skip "Password auth requires SUDO and password file."
fi

# Enable password auth
echo "PasswordAuthentication yes" >>sshd_proxy

# Create askpass script to replay a series of password responses.
# Keep a counter of the number of times it has been called and
# reply with the next line of the replypass file.
cat >${OBJ}/replypass.sh <<EOD
#!/bin/sh
n=\`cat ${OBJ}/replypass.N\`
awk "NR==\$n" ${OBJ}/replypass
echo \$(( \$n + 1 )) >${OBJ}/replypass.N
EOD
chmod 700 ${OBJ}/replypass.sh

SSH_ASKPASS=${OBJ}/replypass.sh
SSH_ASKPASS_REQUIRE=force
export SSH_ASKPASS SSH_ASKPASS_REQUIRE

opts="-oPasswordAuthentication=yes -oPreferredAuthentications=password"
opts="-oBatchMode=no $opts"

trace plain password
cat ${OBJ}/password >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -ne 0 ]; then
	fail "ssh password failed"
fi

trace 2-round password
(echo; cat ${OBJ}/password) >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -ne 0 ]; then
	fail "ssh 2-round password failed"
fi

trace empty password
echo >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -eq 0 ]; then
	fail "ssh password failed"
fi
