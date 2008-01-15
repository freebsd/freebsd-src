#!/bin/sh
#
# Copyright (c) 2003 Dan Nelson
# All rights reserved.
#
# Please see src/share/examples/etc/bsd-style-copyright.
#
# $FreeBSD: src/usr.sbin/mtree/test/test02.sh,v 1.1 2003/10/31 13:39:19 phk Exp $
#

set -e

TMP=/tmp/mtree.$$

rm -rf ${TMP}
mkdir -p ${TMP} ${TMP}/mr ${TMP}/mt

touch -t 199901020304 ${TMP}/mr/oldfile
touch ${TMP}/mt/oldfile

mtree -c -p ${TMP}/mr > ${TMP}/_ 

mtree -U -r -p ${TMP}/mt < ${TMP}/_ > /dev/null

x=x`(cd ${TMP}/mr ; ls -l 2>&1) || true`
y=x`(cd ${TMP}/mt ; ls -l 2>&1) || true`

if [ "$x" != "$y" ] ; then
	echo "ERROR Update of mtime failed" 1>&2
	rm -rf ${TMP}
	exit 1
fi

rm -rf ${TMP}
exit 0

