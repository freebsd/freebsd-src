#	$OpenBSD: putty-ciphers.sh,v 1.11 2021/09/01 03:16:06 dtucker Exp $
#	Placed in the Public Domain.

tid="putty ciphers"

if test "x$REGRESS_INTEROP_PUTTY" != "xyes" ; then
	skip "putty interop tests not enabled"
fi

# Re-enable ssh-rsa on older PuTTY versions.
oldver="`${PLINK} --version | awk '/plink: Release/{if ($3<0.76)print "yes"}'`"
if [ "x$oldver" = "xyes" ]; then
	echo "HostKeyAlgorithms +ssh-rsa" >> ${OBJ}/sshd_proxy
	echo "PubkeyAcceptedKeyTypes +ssh-rsa" >> ${OBJ}/sshd_proxy
fi

for c in aes 3des aes128-ctr aes192-ctr aes256-ctr chacha20 ; do
	verbose "$tid: cipher $c"
	cp ${OBJ}/.putty/sessions/localhost_proxy \
	    ${OBJ}/.putty/sessions/cipher_$c
	echo "Cipher=$c" >> ${OBJ}/.putty/sessions/cipher_$c

	rm -f ${COPY}
	env HOME=$PWD ${PLINK} -load cipher_$c -batch -i ${OBJ}/putty.rsa2 \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
done
rm -f ${COPY}

