#	$OpenBSD: ssh-com-sftp.sh,v 1.3 2002/07/16 08:58:16 markus Exp $
#	Placed in the Public Domain.

tid="basic sftp put/get with ssh.com server"

DATA=/bin/ls
COPY=${OBJ}/copy

BUFFERSIZE="5 1000 32000 64000"
REQUESTS="1 2 10"

#TEST_COMBASE=/path/to/ssh/com/binaries
if [ "X${TEST_COMBASE}" = "X" ]; then
	fatal '$TEST_COMBASE is not set'
fi

VERSIONS="
	2.0.10
	2.0.12
	2.0.13
	2.1.0
	2.2.0
	2.3.0
	2.3.1
	2.4.0
	3.0.0
	3.1.0
	3.2.0
	3.3.0"

# go for it
for v in ${VERSIONS}; do
	server=${TEST_COMBASE}/${v}/sftp-server2
	if [ ! -x ${server} ]; then
		continue
	fi
	verbose "sftp-server $v"
	for B in ${BUFFERSIZE}; do
		for R in ${REQUESTS}; do
			verbose "test $tid: buffer_size $B num_requests $R"
			rm -f ${COPY}.1 ${COPY}.2
			${SFTP} -P ${server} -B $B -R $R -b /dev/stdin \
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
done
