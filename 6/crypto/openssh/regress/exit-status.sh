#	$OpenBSD: exit-status.sh,v 1.6 2002/03/15 13:08:56 markus Exp $
#	Placed in the Public Domain.

tid="remote exit status"

for p in 1 2; do
	for s in 0 1 4 5 44; do
		trace "proto $p status $s"
		verbose "test $tid: proto $p status $s"
		${SSH} -$p -F $OBJ/ssh_proxy otherhost exit $s
		r=$?
		if [ $r -ne $s ]; then
			fail "exit code mismatch for protocol $p: $r != $s"
		fi

		# same with early close of stdout/err
		${SSH} -$p -F $OBJ/ssh_proxy -n otherhost \
                	exec sh -c \'"sleep 2; exec > /dev/null 2>&1; sleep 3; exit $s"\'
		r=$?
		if [ $r -ne $s ]; then
			fail "exit code (with sleep) mismatch for protocol $p: $r != $s"
		fi
	done
done
