#	$OpenBSD: putty-kex.sh,v 1.9 2021/09/01 03:16:06 dtucker Exp $
#	Placed in the Public Domain.

tid="putty KEX"

if test "x$REGRESS_INTEROP_PUTTY" != "xyes" ; then
	skip "putty interop tests not enabled"
fi

# Re-enable ssh-rsa on older PuTTY versions.
oldver="`${PLINK} --version | awk '/plink: Release/{if ($3<0.76)print "yes"}'`"
if [ "x$oldver" = "xyes" ]; then
	echo "HostKeyAlgorithms +ssh-rsa" >> ${OBJ}/sshd_proxy
	echo "PubkeyAcceptedKeyTypes +ssh-rsa" >> ${OBJ}/sshd_proxy
fi

for k in dh-gex-sha1 dh-group1-sha1 dh-group14-sha1 ecdh ; do
	verbose "$tid: kex $k"
	cp ${OBJ}/.putty/sessions/localhost_proxy \
	    ${OBJ}/.putty/sessions/kex_$k
	echo "KEX=$k" >> ${OBJ}/.putty/sessions/kex_$k

	env HOME=$PWD ${PLINK} -load kex_$k -batch -i ${OBJ}/putty.rsa2 true
	if [ $? -ne 0 ]; then
		fail "KEX $k failed"
	fi
done

