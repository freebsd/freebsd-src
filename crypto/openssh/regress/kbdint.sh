#	$OpenBSD: kbdint.sh,v 1.2 2026/02/24 00:39:59 dtucker Exp $
#	Placed in the Public Domain.
#
# This tests keyboard-interactive authentication.  It does not run by default,
# and needs to be enabled by putting the password of the user running the tests
# into ${OBJ}/kbdintpw.  Since this obviously puts the password at risk it is
# recommended to do this on a throwaway VM by setting a random password
# (and randomizing it again after the test, if you can't immediately dispose
# of the VM).

tid="kbdint"

if [ -z "$SUDO" -o ! -f ${OBJ}/kbdintpw ]; then
	skip "Password auth requires SUDO and kbdintpw file."
fi

# Enable keyboard-interactive auth
echo "KbdInteractiveAuthentication yes" >>sshd_proxy

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

opts="-oKbdInteractiveAuthentication=yes -oPreferredAuthentications=keyboard-interactive"
opts="-oBatchMode=no $opts"

trace correct password 1st attempt
cat ${OBJ}/kbdintpw >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -ne 0 ]; then
	fail "ssh kdbint failed"
fi

trace bad password
echo badpass >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -eq 0 ]; then
	fail "ssh unexpectedly succeeded"
fi

trace correct password 2nd attempt
(echo badpass; cat ${OBJ}/kbdintpw) >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -ne 0 ]; then
	fail "did not succeed on 2nd attempt"
fi

trace empty password
echo >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -eq 0 ]; then
	fail "ssh unexpectedly succeeded with empty password"
fi

trace huge password
(for i in 0 1 2 3 4 5 6 7 8 9; do printf 0123456789; done; echo) \
    >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -eq 0 ]; then
	fail "ssh unexpectedly succeeded with huge password"
fi

trace spam password
for i in 0 1 2 3 4 5 6 7 8 9; do printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n'; done \
    >${OBJ}/replypass
echo 1 >${OBJ}/replypass.N
${SSH} $opts -F $OBJ/ssh_proxy somehost true
if [ $? -eq 0 ]; then
	fail "ssh unexpectedly succeeded with password spam"
fi
