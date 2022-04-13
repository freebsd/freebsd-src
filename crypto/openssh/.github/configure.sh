#!/bin/sh

. .github/configs $1

printf "$ "

if [ "x$CC" != "x" ]; then
	printf "CC='$CC' "
fi
if [ "x$CFLAGS" != "x" ]; then
	printf "CFLAGS='$CFLAGS' "
fi
if [ "x$CPPFLAGS" != "x" ]; then
	printf "CPPFLAGS='$CPPFLAGS' "
fi
if [ "x$LDFLAGS" != "x" ]; then
	printf "LDFLAGS='$LDFLAGS' "
fi

echo ./configure ${CONFIGFLAGS}
./configure ${CONFIGFLAGS}
