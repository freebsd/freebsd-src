#	$OpenBSD: sftp-cmds.sh,v 1.2 2003/01/10 07:52:41 djm Exp $
#	Placed in the Public Domain.

# XXX - TODO: 
# - globbed operations
# - chmod / chown / chgrp
# - -p flag for get & put

tid="sftp commands"

DATA=/bin/ls
COPY=${OBJ}/copy

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${BATCH}.*

verbose "$tid: lls"
echo "lls ${OBJ}" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "lls failed"
# XXX always successful

verbose "$tid: ls"
echo "ls ${OBJ}" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "ls failed"
# XXX always successful

verbose "$tid: shell"
echo "!echo hi there" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "shell failed"
# XXX always successful

verbose "$tid: pwd"
echo "pwd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "pwd failed"
# XXX always successful

verbose "$tid: lpwd"
echo "lpwd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "lpwd failed"
# XXX always successful

verbose "$tid: quit"
echo "quit" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "quit failed"
# XXX always successful

verbose "$tid: help"
echo "help" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "help failed"
# XXX always successful

rm -f ${COPY}
verbose "$tid: get"
echo "get $DATA $COPY" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "get failed"
cmp $DATA ${COPY} || fail "corrupted copy after get"

rm -f ${COPY}
verbose "$tid: put"
echo "put $DATA $COPY" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
cmp $DATA ${COPY} || fail "corrupted copy after put"

verbose "$tid: rename"
echo "rename $COPY ${COPY}.1" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rename failed"
test -f ${COPY}.1 || fail "missing file after rename"
cmp $DATA ${COPY}.1 >/dev/null 2>&1 || fail "corrupted copy after rename"

verbose "$tid: ln"
echo "ln ${COPY}.1 ${COPY}.2" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 || fail "ln failed"
test -L ${COPY}.2 || fail "missing file after ln"

verbose "$tid: mkdir"
echo "mkdir ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "mkdir failed"
test -d ${COPY}.dd || fail "missing directory after mkdir"

# XXX do more here
verbose "$tid: chdir"
echo "chdir ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "chdir failed"

verbose "$tid: rmdir"
echo "rmdir ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rmdir failed"
test -d ${COPY}.1 && fail "present directory after rmdir"

verbose "$tid: lmkdir"
echo "lmkdir ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "lmkdir failed"
test -d ${COPY}.dd || fail "missing directory after lmkdir"

# XXX do more here
verbose "$tid: lchdir"
echo "lchdir ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "lchdir failed"

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${BATCH}.*


