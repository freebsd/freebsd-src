#	$OpenBSD: sftp.sh,v 1.2 2002/03/27 22:39:52 markus Exp $
#	Placed in the Public Domain.

tid="basic sftp put/get"

DATA=/bin/ls
COPY=${OBJ}/copy

BUFFERSIZE="5 1000 32000 64000"
REQUESTS="1 2 10"

for B in ${BUFFERSIZE}; do
	for R in ${REQUESTS}; do
                verbose "test $tid: buffer_size $B num_requests $R"
		rm -f ${COPY}.1 ${COPY}.2
		${SFTP} -P ${SFTPSERVER} -B $B -R $R -b /dev/stdin \
		> /dev/null 2>&1 << EOF
		version
		get $DATA ${COPY}.1
		put $DATA ${COPY}.2
EOF
		r=$?
		if [ $r -ne 0 ]; then
			fail "sftp failed with $r"
		fi
		cmp $DATA ${COPY}.1 || fail "corrupted copy after get"
		cmp $DATA ${COPY}.2 || fail "corrupted copy after put"
	done
done
