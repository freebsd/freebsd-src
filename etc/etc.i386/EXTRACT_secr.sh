#!/bin/sh
#
# This file will extract all of the FreeBSD secure distribution into
# ${EXTRACT_TARGET} if it is set, or / otherwise.
#
SOURCEDIR=.
if [ X"${EXTRACT_TARGET}" = X"" ]; then
	EXTRACT_TARGET=/
fi

cd $SOURCEDIR

cat des_tgz.*	| gunzip | tar --directory ${EXTRACT_TARGET} -xpf -
cat libcrypt.aa	| gunzip | tar --directory ${EXTRACT_TARGET} -xpf -
