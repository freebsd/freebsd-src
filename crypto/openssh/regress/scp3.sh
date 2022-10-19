#	$OpenBSD: scp3.sh,v 1.3 2021/08/10 03:35:45 djm Exp $
#	Placed in the Public Domain.

tid="scp3"

#set -x

COPY2=${OBJ}/copy2
DIR=${COPY}.dd
DIR2=${COPY}.dd2

$SSH -F $OBJ/ssh_proxy somehost \
    'IFS=":"; for i in $PATH;do [ -x "$i/scp" ] && exit 0; done; exit 1'
if [ $? -eq 1 ]; then
	skip "No scp on remote path."
fi

SRC=`dirname ${SCRIPT}`
cp ${SRC}/scp-ssh-wrapper.sh ${OBJ}/scp-ssh-wrapper.scp
chmod 755 ${OBJ}/scp-ssh-wrapper.scp
export SCP # used in scp-ssh-wrapper.scp

scpclean() {
	rm -rf ${COPY} ${COPY2} ${DIR} ${DIR2}
	mkdir ${DIR} ${DIR2}
	chmod 755 ${DIR} ${DIR2}
}

for mode in scp sftp ; do
	scpopts="-F${OBJ}/ssh_proxy -S ${SSH} -q"
	tag="$tid: $mode mode"
	if test $mode = scp ; then
		scpopts="$scpopts -O"
	else
		scpopts="-s -D ${SFTPSERVER}"
	fi

	verbose "$tag: simple copy remote file to remote file"
	scpclean
	$SCP $scpopts -3 hostA:${DATA} hostB:${COPY} || fail "copy failed"
	cmp ${DATA} ${COPY} || fail "corrupted copy"

	verbose "$tag: simple copy remote file to remote dir"
	scpclean
	cp ${DATA} ${COPY}
	$SCP $scpopts -3 hostA:${COPY} hostB:${DIR} || fail "copy failed"
	cmp ${COPY} ${DIR}/copy || fail "corrupted copy"

	verbose "$tag: recursive remote dir to remote dir"
	scpclean
	rm -rf ${DIR2}
	cp ${DATA} ${DIR}/copy
	$SCP $scpopts -3r hostA:${DIR} hostB:${DIR2} || fail "copy failed"
	diff -r ${DIR} ${DIR2} || fail "corrupted copy"
	diff -r ${DIR2} ${DIR} || fail "corrupted copy"

	verbose "$tag: detect non-directory target"
	scpclean
	echo a > ${COPY}
	echo b > ${COPY2}
	$SCP $scpopts -3 hostA:${DATA} hostA:${COPY} hostB:${COPY2}
	cmp ${COPY} ${COPY2} >/dev/null && fail "corrupt target"
done

scpclean
rm -f ${OBJ}/scp-ssh-wrapper.exe
