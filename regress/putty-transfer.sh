#	$OpenBSD: putty-transfer.sh,v 1.4 2016/11/25 03:02:01 dtucker Exp $
#	Placed in the Public Domain.

tid="putty transfer data"

if test "x$REGRESS_INTEROP_PUTTY" != "xyes" ; then
	echo "putty interop tests not enabled"
	exit 0
fi

# XXX support protocol 1 too
for p in 2; do
	for c in 0 1 ; do 
	verbose "$tid: proto $p compression $c"
		rm -f ${COPY}
		cp ${OBJ}/.putty/sessions/localhost_proxy \
		    ${OBJ}/.putty/sessions/compression_$c
		echo "Compression=$c" >> ${OBJ}/.putty/sessions/kex_$k
		env HOME=$PWD ${PLINK} -load compression_$c -batch \
		    -i putty.rsa$p cat ${DATA} > ${COPY}
		if [ $? -ne 0 ]; then
			fail "ssh cat $DATA failed"
		fi
		cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	
		for s in 10 100 1k 32k 64k 128k 256k; do
			trace "proto $p compression $c dd-size ${s}"
			rm -f ${COPY}
			dd if=$DATA obs=${s} 2> /dev/null | \
				env HOME=$PWD ${PLINK} -load compression_$c \
				    -batch -i putty.rsa$p \
				    "cat > ${COPY}"
			if [ $? -ne 0 ]; then
				fail "ssh cat $DATA failed"
			fi
			cmp $DATA ${COPY}	|| fail "corrupted copy"
		done
	done
done
rm -f ${COPY}

