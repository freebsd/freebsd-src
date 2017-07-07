#!/bin/sh

POSSIBILITIES='
/usr/local/bin/gmake
/usr/local/bin/make
'

for file in $POSSIBILITIES; do
	if [ -f $file ]; then
		echo $file
		exit 0
	fi
done

echo gmake
echo '$0 could not find make!' 1>&2
exit 1

