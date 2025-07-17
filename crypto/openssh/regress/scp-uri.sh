#	$OpenBSD: scp-uri.sh,v 1.5 2023/01/13 04:47:34 dtucker Exp $
#	Placed in the Public Domain.

tid="scp-uri"

#set -x

COPY2=${OBJ}/copy2
DIR=${COPY}.dd
DIR2=${COPY}.dd2

maybe_add_scp_path_to_sshd

SRC=`dirname ${SCRIPT}`
cp ${SRC}/scp-ssh-wrapper.sh ${OBJ}/scp-ssh-wrapper.scp
chmod 755 ${OBJ}/scp-ssh-wrapper.scp
export SCP # used in scp-ssh-wrapper.scp

scpclean() {
	rm -rf ${COPY} ${COPY2} ${DIR} ${DIR2}
	mkdir ${DIR} ${DIR2}
}

# Remove Port and User from ssh_config, we want to rely on the URI
cp $OBJ/ssh_config $OBJ/ssh_config.orig
egrep -v '^	+(Port|User)	+.*$' $OBJ/ssh_config.orig > $OBJ/ssh_config

for mode in scp sftp ; do
	tag="$tid: $mode mode"
	if test $mode = scp ; then
		scpopts="-O -q -S ${OBJ}/scp-ssh-wrapper.scp"
	else
		scpopts="-s -D ${SFTPSERVER}"
	fi
	verbose "$tag: simple copy local file to remote file"
	scpclean
	$SCP $scpopts ${DATA} "scp://${USER}@somehost:${PORT}/${COPY}" || fail "copy failed"
	cmp ${DATA} ${COPY} || fail "corrupted copy"

	verbose "$tag: simple copy remote file to local file"
	scpclean
	$SCP $scpopts "scp://${USER}@somehost:${PORT}/${DATA}" ${COPY} || fail "copy failed"
	cmp ${DATA} ${COPY} || fail "corrupted copy"

	verbose "$tag: simple copy local file to remote dir"
	scpclean
	cp ${DATA} ${COPY}
	$SCP $scpopts ${COPY} "scp://${USER}@somehost:${PORT}/${DIR}" || fail "copy failed"
	cmp ${COPY} ${DIR}/copy || fail "corrupted copy"

	verbose "$tag: simple copy remote file to local dir"
	scpclean
	cp ${DATA} ${COPY}
	$SCP $scpopts "scp://${USER}@somehost:${PORT}/${COPY}" ${DIR} || fail "copy failed"
	cmp ${COPY} ${DIR}/copy || fail "corrupted copy"

	verbose "$tag: recursive local dir to remote dir"
	scpclean
	rm -rf ${DIR2}
	cp ${DATA} ${DIR}/copy
	$SCP $scpopts -r ${DIR} "scp://${USER}@somehost:${PORT}/${DIR2}" || fail "copy failed"
	for i in $(cd ${DIR} && echo *); do
		cmp ${DIR}/$i ${DIR2}/$i || fail "corrupted copy"
	done

	verbose "$tag: recursive remote dir to local dir"
	scpclean
	rm -rf ${DIR2}
	cp ${DATA} ${DIR}/copy
	$SCP $scpopts -r "scp://${USER}@somehost:${PORT}/${DIR}" ${DIR2} || fail "copy failed"
	for i in $(cd ${DIR} && echo *); do
		cmp ${DIR}/$i ${DIR2}/$i || fail "corrupted copy"
	done

	# TODO: scp -3
done

scpclean
rm -f ${OBJ}/scp-ssh-wrapper.exe
