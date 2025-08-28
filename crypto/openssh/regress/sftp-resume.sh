#	$OpenBSD: sftp-resume.sh,v 1.2 2025/03/11 09:06:50 dtucker Exp $
#	Placed in the Public Domain.

tid="sftp resume"

CLIENT_LOG=${OBJ}/sftp.log

# We test up to 1MB, ensure source data is large enough.
increase_datafile_size 1200

for cmd in put get; do
    verbose "$tid: ${cmd}"
    for size in 0 1 1k 1m size-1 same; do
	trace "$tid: test ${cmd} ${size}"
	rm -rf ${COPY}.1 ${COPY}.2
	cp ${DATA} ${COPY}.1

	# Set up requested source and destination file sizes.
	case "${size}" in
	0)	touch ${COPY}.2
		;;
	size-1)	dd if=${DATA} of=${COPY}.1 bs=1024 count=1 >/dev/null 2>&1
		dd if=${DATA} of=${COPY}.2 bs=1023 count=1 >/dev/null 2>&1
		;;
	same)	cp ${DATA} ${COPY}.2
		;;
	1m)	dd if=${COPY}.1 of=${COPY}.2 bs=1k count=1k >/dev/null 2<&1
		;;
	*)	dd if=${COPY}.1 of=${COPY}.2 bs=${size} count=1 >/dev/null 2>&1
		;;
	esac

	# Perform copy and check.
	echo "${cmd} -a ${COPY}.1 ${COPY}.2" | \
	    ${SFTP} -D ${SFTPSERVER} -vvv >${CLIENT_LOG} 2>&1 \
	    || fail "${cmd} failed"
	cmp ${COPY}.1 ${COPY}.2 || fail "corrupted copy after ${cmd} ${size}"
	grep "reordered" ${CLIENT_LOG} >/dev/null && \
	    fail "server reordered requests ${cmd} ${size}"
    done
done

rm -rf ${COPY}.1 ${COPY}.2
