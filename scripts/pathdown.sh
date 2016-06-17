#!/bin/sh
UP=
DN=${PWD:?}
TP=${TOPDIR:?}

while [ ! $TP/$UP/. -ef $DN ] ;do
	UP=`basename $PWD`/$UP
	cd ..
	if [ "$PWD" = "/" ]; then echo "Lost"; exit 1; fi
done

echo $UP
exit 0
