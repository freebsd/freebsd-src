#	$OpenBSD: yes-head.sh,v 1.4 2002/03/15 13:08:56 markus Exp $
#	Placed in the Public Domain.

tid="yes pipe head"

for p in 1 2; do
	lines=`${SSH} -$p -F $OBJ/ssh_proxy thishost 'yes | head -2000' | (sleep 3 ; wc -l)`
	if [ $? -ne 0 ]; then
		fail "yes|head test failed"
		lines = 0;
	fi
	if [ $lines -ne 2000 ]; then
		fail "yes|head returns $lines lines instead of 2000"
	fi
done
