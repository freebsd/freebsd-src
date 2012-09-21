#!/bin/sh
#
# Copyright (c) 2003 Poul-Henning Kamp
# All rights reserved.
#
# Please see src/share/examples/etc/bsd-style-copyright.
#
# $FreeBSD$
#

set -e

TMP=/tmp/mtree.$$

rm -rf ${TMP}
mkdir -p ${TMP}
echo ${TMP}

K=uid,uname,gid,gname,flags,size,cksum

rm -rf _FOO
mkdir _FOO
touch _FOO/_uid
touch _FOO/_size
touch _FOO/zztype

touch _FOO/_bar
nmtree -c -K $K -p .. > ${TMP}/_r
nmtree -c -K $K -p .. > ${TMP}/_r2
rm -rf _FOO/_bar 

rm -rf _FOO/zztype
mkdir _FOO/zztype

date > _FOO/_size

chown nobody _FOO/_uid

touch _FOO/_foo
nmtree -c -K $K -p .. > ${TMP}/_t

rm -fr _FOO

if nmtree -f ${TMP}/_r -f ${TMP}/_r2 ; then
	true
else
	echo "ERROR Compare identical failed" 1>&2
	exit 1
fi
	
if nmtree -f ${TMP}/_r -f ${TMP}/_t > ${TMP}/_ ; then
	echo "ERROR Compare different succeeded" 1>&2
	exit 1
fi

if [ `wc -l  < ${TMP}/_` -ne 10 ] ; then
	echo "ERROR wrong number of lines: `wc -l  ${TMP}/_`" 1>&2
	exit 1
fi
	
exit 0
