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
mkdir -p ${TMP} ${TMP}/mr ${TMP}/mt


mkdir ${TMP}/mt/foo
mkdir ${TMP}/mr/\*
nmtree -c -p ${TMP}/mr | nmtree -U -r -p ${TMP}/mt > /dev/null 2>&1
if [ -d ${TMP}/mt/foo ] ; then
	echo "ERROR Mtree create fell for filename with '*' char" 1>&2
	rm -rf ${TMP}
	exit 1
fi
rmdir ${TMP}/mr/\*

mkdir -p ${TMP}/mt/foo
mkdir ${TMP}/mr/\[f\]oo
nmtree -c -p ${TMP}/mr | nmtree -U -r -p ${TMP}/mt > /dev/null 2>&1
if [ -d ${TMP}/mt/foo ] ; then
	echo "ERROR Mtree create fell for filename with '[' char" 1>&2
	rm -rf ${TMP}
	exit 1
fi
rmdir ${TMP}/mr/\[f\]oo

mkdir -p ${TMP}/mt/foo
mkdir ${TMP}/mr/\?oo
nmtree -c -p ${TMP}/mr | nmtree -U -r -p ${TMP}/mt > /dev/null 2>&1
if [ -d ${TMP}/mt/foo ] ; then
	echo "ERROR Mtree create fell for filename with '?' char" 1>&2
	rm -rf ${TMP}
	exit 1
fi
rmdir ${TMP}/mr/\?oo

mkdir ${TMP}/mr/\#
nmtree -c -p ${TMP}/mr > ${TMP}/_
if nmtree -U -r -p ${TMP}/mt < ${TMP}/_ > /dev/null 2>&1 ; then
	true
else
	echo "ERROR Mtree create fell for filename with '#' char" 1>&2
	rm -rf ${TMP}
	exit 1
fi
	
if [ ! -d ${TMP}/mt/\# ] ; then
	echo "ERROR Mtree update failed to create name with '#' char" 1>&2
	rm -rf ${TMP}
	exit 1
fi
rmdir ${TMP}/mr/\#

rm -rf ${TMP}
exit 0
