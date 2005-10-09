#	$OpenBSD: sftp-cmds.sh,v 1.6 2003/10/07 07:04:52 djm Exp $
#	Placed in the Public Domain.

# XXX - TODO: 
# - chmod / chown / chgrp
# - -p flag for get & put

tid="sftp commands"

DATA=/bin/ls${EXEEXT}
COPY=${OBJ}/copy
# test that these files are readable!
for i in `(cd /bin;echo l*)`
do
	if [ -r $i ]; then
		GLOBFILES="$GLOBFILES $i"
	fi
done

if have_prog uname
then
	case `uname` in
	CYGWIN*)
		os=cygwin
		;;
	*)
		os=`uname`
		;;
	esac
else
	os="unknown"
fi

# Path with embedded quote
QUOTECOPY=${COPY}".\"blah\""
QUOTECOPY_ARG=${COPY}'.\"blah\"'

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${COPY}.dd2
mkdir ${COPY}.dd

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
verbose "$tid: get quoted"
echo "get \"$DATA\" $COPY" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "get failed"
cmp $DATA ${COPY} || fail "corrupted copy after get"

if [ "$os" != "cygwin" ]; then
rm -f ${QUOTECOPY}
cp $DATA ${QUOTECOPY}
verbose "$tid: get filename with quotes"
echo "get \"$QUOTECOPY_ARG\" ${COPY}" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
cmp ${COPY} ${QUOTECOPY} || fail "corrupted copy after get with quotes"
rm -f ${QUOTECOPY} ${COPY}
fi

rm -f ${COPY}.dd/*
verbose "$tid: get to directory"
echo "get $DATA ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
        || fail "get failed"
cmp $DATA ${COPY}.dd/`basename $DATA` || fail "corrupted copy after get"

rm -f ${COPY}.dd/*
verbose "$tid: glob get to directory"
echo "get /bin/l* ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
        || fail "get failed"
for x in $GLOBFILES; do
        cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after get"
done

rm -f ${COPY}.dd/*
verbose "$tid: get to local dir"
(echo "lcd ${COPY}.dd"; echo "get $DATA" ) | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
        || fail "get failed"
cmp $DATA ${COPY}.dd/`basename $DATA` || fail "corrupted copy after get"

rm -f ${COPY}.dd/*
verbose "$tid: glob get to local dir"
(echo "lcd ${COPY}.dd"; echo "get /bin/l*") | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
        || fail "get failed"
for x in $GLOBFILES; do
        cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after get"
done

rm -f ${COPY}
verbose "$tid: put"
echo "put $DATA $COPY" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
cmp $DATA ${COPY} || fail "corrupted copy after put"

if [ "$os" != "cygwin" ]; then
rm -f ${QUOTECOPY}
verbose "$tid: put filename with quotes"
echo "put $DATA \"$QUOTECOPY_ARG\"" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
cmp $DATA ${QUOTECOPY} || fail "corrupted copy after put with quotes"
fi

rm -f ${COPY}.dd/*
verbose "$tid: put to directory"
echo "put $DATA ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
cmp $DATA ${COPY}.dd/`basename $DATA` || fail "corrupted copy after put"

rm -f ${COPY}.dd/*
verbose "$tid: glob put to directory"
echo "put /bin/l* ${COPY}.dd" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
for x in $GLOBFILES; do
	cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after put"
done

rm -f ${COPY}.dd/*
verbose "$tid: put to local dir"
(echo "cd ${COPY}.dd"; echo "put $DATA") | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
cmp $DATA ${COPY}.dd/`basename $DATA` || fail "corrupted copy after put"

rm -f ${COPY}.dd/*
verbose "$tid: glob put to local dir"
(echo "cd ${COPY}.dd"; echo "put /bin/l*") | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "put failed"
for x in $GLOBFILES; do
        cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after put"
done

verbose "$tid: rename"
echo "rename $COPY ${COPY}.1" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rename failed"
test -f ${COPY}.1 || fail "missing file after rename"
cmp $DATA ${COPY}.1 >/dev/null 2>&1 || fail "corrupted copy after rename"

verbose "$tid: rename directory"
echo "rename ${COPY}.dd ${COPY}.dd2" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 \
	|| fail "rename directory failed"
test -d ${COPY}.dd && fail "oldname exists after rename directory"
test -d ${COPY}.dd2 || fail "missing newname after rename directory"

verbose "$tid: ln"
echo "ln ${COPY}.1 ${COPY}.2" | ${SFTP} -P ${SFTPSERVER} >/dev/null 2>&1 || fail "ln failed"
test -h ${COPY}.2 || fail "missing file after ln"

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

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${COPY}.dd2


