#	$OpenBSD: authinfo.sh,v 1.1 2017/06/24 06:35:24 djm Exp $
#	Placed in the Public Domain.

tid="authinfo"

# Ensure the environment variable doesn't leak when ExposeAuthInfo=no.
verbose "ExposeAuthInfo=no"
env SSH_USER_AUTH=blah ${SSH} -F $OBJ/ssh_proxy x \
	'test -z "$SSH_USER_AUTH"' || fail "SSH_USER_AUTH present"

verbose "ExposeAuthInfo=yes"
echo ExposeAuthInfo=yes >> $OBJ/sshd_proxy
${SSH} -F $OBJ/ssh_proxy x \
	'grep ^publickey "$SSH_USER_AUTH" /dev/null >/dev/null' ||
	fail "ssh with ExposeAuthInfo failed"

# XXX test multiple auth and key contents
