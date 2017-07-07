#!/bin/sh
#
#

DIR=@DIR@
AWK=@AWK@
SED=@SED@

set -e
FILE=$1
ROOT=`echo $1 | ${SED} -e s/.ct$//`
BASE=`echo $ROOT | ${SED} -e 's;.*/;;'`
TMP=ct$$.c

if [ ! -r ${FILE} ] ; then
	echo mk_cmds: ${FILE} not found
	exit 1
fi

${SED} -f ${DIR}/ct_c.sed  ${FILE} \
	| ${AWK} -f ${DIR}/ct_c.awk rootname=${ROOT} outfile=${TMP} -

if grep "^#__ERROR_IN_FILE" ${TMP} > /dev/null; then
	rm ${TMP}
	exit 1
else
	mv ${TMP} ${BASE}.c
	exit 0
fi
