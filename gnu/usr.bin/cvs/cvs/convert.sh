#!/bin/sh
#
# short script to convert cvs repositories to support file death.
#

WORKDIR=/tmp/$$-cvs-convert
mkdir ${WORKDIR}
cd ${WORKDIR}

case $# in
1) ;;
*)
	echo Usage: convert repository 2>&1
	exit 1
	;;
esac

attics=`find $1 -name Attic -print`

for i in ${attics} ; do
	mkdir $i/SAVE
	for j in $i/*,v ; do
		echo $j
		cp $j $i/SAVE
		co -l $j
		ci -f -sdead -m"recording file death" $j
	done
done
