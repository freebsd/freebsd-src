#!/bin/sh
# $Id: ibcs2,v 1.1 1994/10/14 08:45:15 sos Exp $
if [ $# -le 1 ]; then
	LOADERS="coff" # elf
fi

set -e

kernelfile=`sysctl -n kern.bootfile`
kernelfile=`basename $kernelfile`
newkernelfile="/tmp/${kernelfile}+ibcs2"

modload -e ibcs2_init -o $newkernelfile -q /lkm/ibcs2_mod.o
for loader in $LOADERS; do
	modload -e${loader}_init -o/tmp/ibcs2_${loader}.o -qu \
		-A${newkernelfile} /lkm/ibcs2_${loader}_mod.o
done
modload -e socksys_init -o $newkernelfile -q /lkm/socksys_mod.o
set +e
