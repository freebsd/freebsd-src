#	$OpenBSD: yes-head.sh,v 1.7 2023/01/14 10:05:54 dtucker Exp $
#	Placed in the Public Domain.

tid="yes pipe head"

lines=`${SSH} -F $OBJ/ssh_proxy thishost 'sh -c "while true;do echo yes;done | _POSIX2_VERSION=199209 head -2000"' | (sleep 3 ; wc -l)`
if [ $? -ne 0 ]; then
	fail "yes|head test failed"
+	lines=0
fi
if [ $lines -ne 2000 ]; then
	fail "yes|head returns $lines lines instead of 2000"
fi
