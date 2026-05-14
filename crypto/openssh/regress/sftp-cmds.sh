#	$OpenBSD: sftp-cmds.sh,v 1.23 2025/10/13 00:55:45 djm Exp $
#	Placed in the Public Domain.

# XXX - TODO: 
# - chmod / chown / chgrp
# - -p flag for get & put

tid="sftp commands"

DIFFOPT="-rN"
# Figure out if diff does not understand "-N"
if ! diff -N ${SRC}/sftp-cmds.sh ${SRC}/sftp-cmds.sh 2>/dev/null; then
	DIFFOPT="-r"
fi

# test that these files are readable!
for i in `(cd /bin;echo l*)`
do
	if [ -r $i ]; then
		GLOBFILES="$GLOBFILES $i"
	fi
done

# Path with embedded quote
QUOTECOPY=${COPY}".\"blah\""
QUOTECOPY_ARG=${COPY}'.\"blah\"'
# File with spaces
SPACECOPY="${COPY} this has spaces.txt"
SPACECOPY_ARG="${COPY}\ this\ has\ spaces.txt"
# File with glob metacharacters
GLOBMETACOPY="${COPY} [metachar].txt"

sftpserver() {
	${SFTP} -D ${SFTPSERVER} >/dev/null 2>&1
}

sftpserver_with_stdout() {
	${SFTP} -D ${SFTPSERVER} 2>&1
}

forest() {
	rm -rf ${COPY}.dd/*
	rm -rf ${COPY}.dd2
	mkdir -p ${COPY}.dd/a ${COPY}.dd/b ${COPY}.dd/c ${COPY}.dd/a/d
	echo 'A' > ${COPY}.dd/a/A
	echo 'B' > ${COPY}.dd/a/B
	echo 'C' > ${COPY}.dd/a/C
	echo 'D' > ${COPY}.dd/a/D
}

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${COPY}.dd2
mkdir ${COPY}.dd

verbose "$tid: lls"
printf "lcd ${OBJ}\nlls\n" | sftpserver_with_stdout | \
	grep copy.dd >/dev/null || fail "lls failed"

verbose "$tid: lls w/path"
echo "lls ${OBJ}" | sftpserver_with_stdout | \
	grep copy.dd >/dev/null || fail "lls w/path failed"

verbose "$tid: ls"
echo "ls ${OBJ}" | sftpserver || fail "ls failed"
# XXX always successful

verbose "$tid: shell"
echo "!echo hi there" | sftpserver_with_stdout | \
	egrep '^hi there$' >/dev/null || fail "shell failed"

verbose "$tid: pwd"
echo "pwd" | sftpserver || fail "pwd failed"
# XXX always successful

verbose "$tid: lpwd"
echo "lpwd" | sftpserver  || fail "lpwd failed"
# XXX always successful

verbose "$tid: quit"
echo "quit" | sftpserver || fail "quit failed"
# XXX always successful

verbose "$tid: help"
echo "help" | sftpserver || fail "help failed"
# XXX always successful

rm -f ${COPY}
verbose "$tid: get"
echo "get $DATA $COPY" | sftpserver || fail "get failed"
cmp $DATA ${COPY} || fail "corrupted copy after get"

rm -f ${COPY}
verbose "$tid: get quoted"
echo "get \"$DATA\" $COPY" | sftpserver || fail "get failed"
cmp $DATA ${COPY} || fail "corrupted copy after get"

rm -f ${QUOTECOPY}
cp $DATA ${QUOTECOPY}
verbose "$tid: get filename with quotes"
echo "get \"$QUOTECOPY_ARG\" ${COPY}" | sftpserver  || fail "get failed"
cmp ${COPY} ${QUOTECOPY} || fail "corrupted copy after get with quotes"
rm -f ${QUOTECOPY} ${COPY}

rm -f "$SPACECOPY" ${COPY}
cp $DATA "$SPACECOPY"
verbose "$tid: get filename with spaces"
echo "get ${SPACECOPY_ARG} ${COPY}" | sftpserver || fail "get failed"
cmp ${COPY} "$SPACECOPY" || fail "corrupted copy after get with spaces"

rm -f "$GLOBMETACOPY" ${COPY}
cp $DATA "$GLOBMETACOPY"
verbose "$tid: get filename with glob metacharacters"
echo "get \"${GLOBMETACOPY}\" ${COPY}" | sftpserver || fail "get failed"
cmp ${COPY} "$GLOBMETACOPY" || \
	fail "corrupted copy after get with glob metacharacters"

rm -rf ${COPY}.dd/*
verbose "$tid: get to directory"
echo "get $DATA ${COPY}.dd" | sftpserver || fail "get failed"
cmp $DATA ${COPY}.dd/${DATANAME} || fail "corrupted copy after get"

rm -rf ${COPY}.dd/*
verbose "$tid: glob get to directory"
echo "get /bin/l* ${COPY}.dd" | sftpserver || fail "get failed"
for x in $GLOBFILES; do
        cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after get"
done

rm -rf ${COPY}.dd/*
verbose "$tid: get to local dir"
printf "lcd ${COPY}.dd\nget $DATA\n" | sftpserver || fail "get failed"
cmp $DATA ${COPY}.dd/${DATANAME} || fail "corrupted copy after get"

rm -rf ${COPY}.dd/*
verbose "$tid: glob get to local dir"
printf "lcd ${COPY}.dd\nget /bin/l*\n" | sftpserver || fail "get failed"
for x in $GLOBFILES; do
        cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after get"
done

forest
verbose "$tid: get recursive absolute"
echo "get -R ${COPY}.dd ${COPY}.dd2" | sftpserver || fail "get failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

forest
verbose "$tid: get recursive relative src"
printf "cd ${COPY}.dd\n get -R . ${COPY}.dd2\n" | sftpserver || \
	fail "get failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

forest
verbose "$tid: get relative .."
printf "cd ${COPY}.dd/b\n get -R .. ${COPY}.dd2\n" | sftpserver || \
	fail "get failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

forest
mkdir ${COPY}.dd2
verbose "$tid: get recursive relative .."
printf "cd ${COPY}.dd/b\n lcd ${COPY}.dd2\n get -R ..\n" | sftpserver || \
	fail "get failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

rm -f ${COPY}
verbose "$tid: put"
echo "put $DATA $COPY" | sftpserver || fail "put failed"
cmp $DATA ${COPY} || fail "corrupted copy after put"

rm -f ${QUOTECOPY}
verbose "$tid: put filename with quotes"
echo "put $DATA \"$QUOTECOPY_ARG\"" | sftpserver || fail "put failed"
cmp $DATA ${QUOTECOPY} || fail "corrupted copy after put with quotes"

rm -f "$SPACECOPY"
verbose "$tid: put filename with spaces"
echo "put $DATA ${SPACECOPY_ARG}" | sftpserver || fail "put failed"
cmp $DATA "$SPACECOPY" || fail "corrupted copy after put with spaces"

rm -rf ${COPY}.dd/*
verbose "$tid: put to directory"
echo "put $DATA ${COPY}.dd" | sftpserver || fail "put failed"
cmp $DATA ${COPY}.dd/${DATANAME} || fail "corrupted copy after put"

rm -rf ${COPY}.dd/*
verbose "$tid: glob put to directory"
echo "put /bin/l? ${COPY}.dd" | sftpserver || fail "put failed"
for x in $GLOBFILES; do
	cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after put"
done

rm -rf ${COPY}.dd/*
verbose "$tid: put to local dir"
printf "cd ${COPY}.dd\nput $DATA\n" | sftpserver || fail "put failed"
cmp $DATA ${COPY}.dd/${DATANAME} || fail "corrupted copy after put"

rm -rf ${COPY}.dd/*
verbose "$tid: glob put to local dir"
printf "cd ${COPY}.dd\nput /bin/l*\n" | sftpserver || fail "put failed"
for x in $GLOBFILES; do
        cmp /bin/$x ${COPY}.dd/$x || fail "corrupted copy after put"
done

forest
verbose "$tid: put recursive absolute"
echo "put -R ${COPY}.dd ${COPY}.dd2" | sftpserver || fail "put failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

forest
verbose "$tid: put recursive relative src"
printf "lcd ${COPY}.dd\n put -R . ${COPY}.dd2\n" | sftpserver || \
	fail "put failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

forest
verbose "$tid: put recursive .."
printf "lcd ${COPY}.dd/b\n put -R .. ${COPY}.dd2\n" | sftpserver || \
	fail "put failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

forest
mkdir ${COPY}.dd2
verbose "$tid: put recursive .. relative"
printf "lcd ${COPY}.dd/b\n cd ${COPY}.dd2\n put -R ..\n" | sftpserver || \
	fail "put failed"
diff ${DIFFOPT} ${COPY}.dd ${COPY}.dd2 || fail "corrupted copy"

verbose "$tid: rename"
echo "rename $COPY ${COPY}.1" | sftpserver || fail "rename failed"
test -f ${COPY}.1 || fail "missing file after rename"
cmp $DATA ${COPY}.1 >/dev/null 2>&1 || fail "corrupted copy after rename"

verbose "$tid: rename directory"
rm -rf ${COPY}.dd2
echo "rename ${COPY}.dd ${COPY}.dd2" | sftpserver || \
	fail "rename directory failed"
test -d ${COPY}.dd && fail "oldname exists after rename directory"
test -d ${COPY}.dd2 || fail "missing newname after rename directory"

verbose "$tid: ln"
echo "ln ${COPY}.1 ${COPY}.2" | sftpserver || fail "ln failed"
test -f ${COPY}.2 || fail "missing file after ln"
cmp ${COPY}.1 ${COPY}.2 || fail "created file is not equal after ln"

verbose "$tid: ln -s"
rm -f ${COPY}.2
echo "ln -s ${COPY}.1 ${COPY}.2" | sftpserver || fail "ln -s failed"
test -h ${COPY}.2 || fail "missing file after ln -s"

verbose "$tid: cp"
rm -f ${COPY}.2
echo "cp ${COPY}.1 ${COPY}.2" | sftpserver || fail "cp failed"
cmp ${COPY}.1 ${COPY}.2 || fail "created file is not equal after cp"

verbose "$tid: mkdir"
echo "mkdir ${COPY}.dd" | sftpserver || fail "mkdir failed"
test -d ${COPY}.dd || fail "missing directory after mkdir"

# XXX do more here
verbose "$tid: chdir"
echo "chdir ${COPY}.dd" | sftpserver || fail "chdir failed"

verbose "$tid: rmdir"
echo "rmdir ${COPY}.dd" | sftpserver || fail "rmdir failed"
test -d ${COPY}.1 && fail "present directory after rmdir"

verbose "$tid: lmkdir"
echo "lmkdir ${COPY}.dd" | sftpserver || fail "lmkdir failed"
test -d ${COPY}.dd || fail "missing directory after lmkdir"

# XXX do more here
verbose "$tid: lchdir"
echo "lchdir ${COPY}.dd" | sftpserver || fail "lchdir failed"

rm -rf ${COPY} ${COPY}.1 ${COPY}.2 ${COPY}.dd ${COPY}.dd2
rm -rf ${QUOTECOPY} "$SPACECOPY" "$GLOBMETACOPY"


