#	$OpenBSD: sftp-glob.sh,v 1.1 2004/12/10 01:31:30 fgsch Exp $
#	Placed in the Public Domain.

tid="sftp glob"

BASE=${OBJ}/glob
DIR=${BASE}/dir
DATA=${DIR}/file

rm -rf ${BASE}
mkdir -p ${DIR}
touch ${DATA}

verbose "$tid: ls file"
echo "ls -l ${DIR}/fil*" | ${SFTP} -P ${SFTPSERVER} 2>/dev/null | \
	grep ${DATA} >/dev/null 2>&1
if [ $? -ne 0 ]; then
	fail "globbed ls file failed"
fi

verbose "$tid: ls dir"
echo "ls -l ${BASE}/d*" | ${SFTP} -P ${SFTPSERVER} 2>/dev/null | \
	grep file >/dev/null 2>&1
if [ $? -ne 0 ]; then
	fail "globbed ls dir failed"
fi

rm -rf ${BASE}
