#	$OpenBSD: scp.sh,v 1.3 2004/07/08 12:59:35 dtucker Exp $
#	Placed in the Public Domain.

tid="scp"

#set -x

# Figure out if diff understands "-N"
if diff -N ${SRC}/scp.sh ${SRC}/scp.sh 2>/dev/null; then
	DIFFOPT="-rN"
else
	DIFFOPT="-r"
fi

DATA=/bin/ls${EXEEXT}
COPY=${OBJ}/copy
COPY2=${OBJ}/copy2
DIR=${COPY}.dd
DIR2=${COPY}.dd2

SRC=`dirname ${SCRIPT}`
cp ${SRC}/scp-ssh-wrapper.sh ${OBJ}/scp-ssh-wrapper.scp
chmod 755 ${OBJ}/scp-ssh-wrapper.scp
scpopts="-q -S ${OBJ}/scp-ssh-wrapper.scp"

scpclean() {
	rm -rf ${COPY} ${COPY2} ${DIR} ${DIR2}
	mkdir ${DIR} ${DIR2}
}

verbose "$tid: simple copy local file to remote file"
scpclean
$SCP $scpopts ${DATA} somehost:${COPY} || fail "copy failed"
cmp ${DATA} ${COPY} || fail "corrupted copy"

verbose "$tid: simple copy remote file to local file"
scpclean
$SCP $scpopts somehost:${DATA} ${COPY} || fail "copy failed"
cmp ${DATA} ${COPY} || fail "corrupted copy"

verbose "$tid: simple copy local file to remote dir"
scpclean
cp ${DATA} ${COPY}
$SCP $scpopts ${COPY} somehost:${DIR} || fail "copy failed"
cmp ${COPY} ${DIR}/copy || fail "corrupted copy"

verbose "$tid: simple copy remote file to local dir"
scpclean
cp ${DATA} ${COPY}
$SCP $scpopts somehost:${COPY} ${DIR} || fail "copy failed"
cmp ${COPY} ${DIR}/copy || fail "corrupted copy"

verbose "$tid: recursive local dir to remote dir"
scpclean
rm -rf ${DIR2}
cp ${DATA} ${DIR}/copy
$SCP $scpopts -r ${DIR} somehost:${DIR2} || fail "copy failed"
diff ${DIFFOPT} ${DIR} ${DIR2} || fail "corrupted copy"

verbose "$tid: recursive remote dir to local dir"
scpclean
rm -rf ${DIR2}
cp ${DATA} ${DIR}/copy
$SCP $scpopts -r somehost:${DIR} ${DIR2} || fail "copy failed"
diff ${DIFFOPT} ${DIR} ${DIR2} || fail "corrupted copy"

if [ ! -z "$SUDO" ]; then
	verbose "$tid: skipped file after scp -p with failed chown+utimes"
	scpclean
	cp -p ${DATA} ${DIR}/copy
	cp -p ${DATA} ${DIR}/copy2
	cp ${DATA} ${DIR2}/copy
	chmod 660 ${DIR2}/copy
	$SUDO chown root ${DIR2}/copy
	$SCP -p $scpopts somehost:${DIR}/\* ${DIR2} >/dev/null 2>&1
	diff ${DIFFOPT} ${DIR} ${DIR2} || fail "corrupted copy"
	$SUDO rm ${DIR2}/copy
fi

for i in 0 1 2 3 4; do
	verbose "$tid: disallow bad server #$i"
	SCPTESTMODE=badserver_$i
	export DIR SCPTESTMODE
	scpclean
	$SCP $scpopts somehost:${DATA} ${DIR} >/dev/null 2>/dev/null
	[ -d {$DIR}/rootpathdir ] && fail "allows dir relative to root dir"
	[ -d ${DIR}/dotpathdir ] && fail "allows dir creation in non-recursive mode"

	scpclean
	$SCP -r $scpopts somehost:${DATA} ${DIR2} >/dev/null 2>/dev/null
	[ -d ${DIR}/dotpathdir ] && fail "allows dir creation outside of subdir"
done

scpclean
rm -f ${OBJ}/scp-ssh-wrapper.scp
