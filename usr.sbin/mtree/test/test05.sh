#!/bin/sh
#
# $FreeBSD: src/usr.sbin/mtree/test/test05.sh,v 1.1.2.1 2006/07/28 16:27:40 maxim Exp $
#
# Test for 'optional' keyword.
#

TMP=`mktemp -d /tmp/mtree.XXXXXX`
mkdir -p ${TMP}/mr ${TMP}/mr/optional-dir ${TMP}/mr/some-dir
touch ${TMP}/mr/optional-file ${TMP}/mr/some-file

mtree -c -p ${TMP}/mr > ${TMP}/_
rm -rf ${TMP}/mr/optional-file ${TMP}/mr/optional-dir
mtree -p ${TMP}/mr -K optional < ${TMP}/_ > /dev/null

res=$?

if [ $res -ne 0 ] ; then
	echo "ERROR 'optional' keyword failed" 1>&2
	rm -rf ${TMP}
	exit 1
fi

rm -rf ${TMP}
exit 0
