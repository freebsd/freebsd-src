#!/bin/sh
#
# This file will extract all of the FreeBSD binaries into ${EXTRACT_TARGET}
# if it is set, or / otherwise.
#
# CAUTION DO NOT USE THIS TO INSTALL THE BINARIES ONTO A RUNNING
# SYSTEM, it will NOT WORK!!!  You should use the extract command from /magic
# for installing the bindist onto your system.
SOURCEDIR=.
if [ X"${EXTRACT_TARGET}" = X"" ]; then
	echo "YOU DO NOT WANT TO DO THAT!!!"
	exit
	EXTRACT_TARGET=/
fi

cd $SOURCEDIR
cat bin_tgz.* | gunzip | tar --directory ${EXTRACT_TARGET} -xpf -
#NO_EXPORT#cat des_tgz.* | gunzip | tar --directory ${EXTRACT_TARGET} -xpf -
