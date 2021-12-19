#	$OpenBSD: knownhosts-command.sh,v 1.3 2021/08/30 01:15:45 djm Exp $
#	Placed in the Public Domain.

tid="known hosts command "

rm -f $OBJ/knownhosts_command $OBJ/ssh_proxy_khc
cp $OBJ/ssh_proxy $OBJ/ssh_proxy_orig

( grep -vi GlobalKnownHostsFile $OBJ/ssh_proxy_orig | \
    grep -vi UserKnownHostsFile;
  echo "GlobalKnownHostsFile none" ;
  echo "UserKnownHostsFile none" ;
  echo "KnownHostsCommand $OBJ/knownhosts_command '%t' '%K' '%u'" ;
) > $OBJ/ssh_proxy

verbose "simple connection"
cat > $OBJ/knownhosts_command << _EOF
#!/bin/sh
cat $OBJ/known_hosts
_EOF
chmod a+x $OBJ/knownhosts_command
${SSH} -F $OBJ/ssh_proxy x true || fail "ssh connect failed"

verbose "no keys"
cat > $OBJ/knownhosts_command << _EOF
#!/bin/sh
exit 0
_EOF
chmod a+x $OBJ/knownhosts_command
${SSH} -F $OBJ/ssh_proxy x true && fail "ssh connect succeeded with no keys"

verbose "bad exit status"
cat > $OBJ/knownhosts_command << _EOF
#!/bin/sh
cat $OBJ/known_hosts
exit 1
_EOF
chmod a+x $OBJ/knownhosts_command
${SSH} -F $OBJ/ssh_proxy x true && fail "ssh connect succeeded with bad exit"

for keytype in ${SSH_HOSTKEY_TYPES} ; do
	algs=$keytype
	test "x$keytype" = "xssh-dss" && continue
	test "x$keytype" = "xssh-rsa" && algs=ssh-rsa,rsa-sha2-256,rsa-sha2-512
	verbose "keytype $keytype"
	cat > $OBJ/knownhosts_command << _EOF
#!/bin/sh
die() { echo "\$@" 1>&2 ; exit 1; }
test "x\$1" = "x$keytype" || die "wrong keytype \$1 (expected $keytype)"
test "x\$3" = "x$LOGNAME" || die "wrong username \$3 (expected $LOGNAME)"
grep -- "\$1.*\$2" $OBJ/known_hosts
_EOF
	${SSH} -F $OBJ/ssh_proxy -oHostKeyAlgorithms=$algs x true ||
	    fail "ssh connect failed for keytype $x"
done
