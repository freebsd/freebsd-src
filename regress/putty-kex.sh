#	$OpenBSD: putty-kex.sh,v 1.5 2020/01/23 03:24:38 dtucker Exp $
#	Placed in the Public Domain.

tid="putty KEX"

if test "x$REGRESS_INTEROP_PUTTY" != "xyes" ; then
	echo "putty interop tests not enabled"
	exit 0
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

