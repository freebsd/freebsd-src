#	$OpenBSD: sftp-glob.sh,v 1.3 2007/10/26 05:30:01 djm Exp $
#	Placed in the Public Domain.

tid="sftp glob"

sftp_ls() {
	target=$1
	errtag=$2
	expected=$3
	unexpected=$4
	verbose "$tid: $errtag"
	printf "ls -l %s" "${target}" | \
		${SFTP} -b - -P ${SFTPSERVER} 2>/dev/null | \
		grep -v "^sftp>" > ${RESULTS}
	if [ $? -ne 0 ]; then
		fail "$errtag failed"
	fi
	if test "x$expected" != "x" ; then
	    if fgrep "$expected" ${RESULTS} >/dev/null 2>&1 ; then
		:
	    else
		fail "$expected missing from $errtag results"
	    fi
	fi
	if test "x$unexpected" != "x" && \
	   fgrep "$unexpected" ${RESULTS} >/dev/null 2>&1 ; then
		fail "$unexpected present in $errtag results"
	fi
	rm -f ${RESULTS}
}

BASE=${OBJ}/glob
RESULTS=${OBJ}/results
DIR=${BASE}/dir
DATA=${DIR}/file

GLOB1="${DIR}/g-wild*"
GLOB2="${DIR}/g-wildx"
QUOTE="${DIR}/g-quote\""
SLASH="${DIR}/g-sl\\ash"
ESLASH="${DIR}/g-slash\\"
QSLASH="${DIR}/g-qs\\\""
SPACE="${DIR}/g-q space"

rm -rf ${BASE}
mkdir -p ${DIR}
touch "${DATA}" "${GLOB1}" "${GLOB2}" "${QUOTE}"
touch "${QSLASH}" "${ESLASH}" "${SLASH}" "${SPACE}"

#       target                   message                expected     unexpected
sftp_ls "${DIR}/fil*"            "file glob"            "${DATA}"    ""
sftp_ls "${BASE}/d*"             "dir glob"             "`basename ${DATA}`" ""
sftp_ls "${DIR}/g-wild\"*\""     "quoted glob"          "g-wild*"    "g-wildx"
sftp_ls "${DIR}/g-wild\*"        "escaped glob"         "g-wild*"    "g-wildx"
sftp_ls "${DIR}/g-quote\\\""     "escaped quote"        "g-quote\""  ""
sftp_ls "\"${DIR}/g-quote\\\"\"" "quoted quote"         "g-quote\""  ""
sftp_ls "'${DIR}/g-quote\"'"     "single-quoted quote"  "g-quote\""  ""
sftp_ls "${DIR}/g-sl\\\\ash"     "escaped slash"        "g-sl\\ash"  ""
sftp_ls "'${DIR}/g-sl\\\\ash'"   "quoted slash"         "g-sl\\ash"  ""
sftp_ls "${DIR}/g-slash\\\\"     "escaped slash at EOL" "g-slash\\"  ""
sftp_ls "'${DIR}/g-slash\\\\'"   "quoted slash at EOL"  "g-slash\\"  ""
sftp_ls "${DIR}/g-qs\\\\\\\""    "escaped slash+quote"  "g-qs\\\""   ""
sftp_ls "'${DIR}/g-qs\\\\\"'"    "quoted slash+quote"   "g-qs\\\""   ""
sftp_ls "${DIR}/g-q\\ space"     "escaped space"        "g-q space"  ""
sftp_ls "'${DIR}/g-q space'"     "quoted space"         "g-q space"  ""

rm -rf ${BASE}

