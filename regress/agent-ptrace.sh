#	$OpenBSD: agent-ptrace.sh,v 1.1 2002/12/09 15:38:30 markus Exp $
#	Placed in the Public Domain.

tid="disallow agent ptrace attach"

if have_prog uname ; then
	case `uname` in
	AIX|CYGWIN*|OSF1)
		echo "skipped (not supported on this platform)"
		exit 0
		;;
	esac
fi

if have_prog gdb ; then
	: ok
else
	echo "skipped (gdb not found)"
	exit 0
fi

if test -z "$SUDO" ; then
	echo "skipped (SUDO not set)"
	exit 0
else
	$SUDO chown 0 ${SSHAGENT}
	$SUDO chgrp 0 ${SSHAGENT}
	$SUDO chmod 2755 ${SSHAGENT}
fi

trace "start agent"
eval `${SSHAGENT} -s` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	# ls -l ${SSH_AUTH_SOCK}
	gdb ${SSHAGENT} ${SSH_AGENT_PID} > ${OBJ}/gdb.out 2>&1 << EOF
		quit
EOF
	if [ $? -ne 0 ]; then
		fail "gdb failed: exit code $?"
	fi
	egrep 'ptrace: Operation not permitted.|procfs:.*Permission denied.|ttrace.*Permission denied.|procfs:.*: Invalid argument.' >/dev/null ${OBJ}/gdb.out
	r=$?
	rm -f ${OBJ}/gdb.out
	if [ $r -ne 0 ]; then
		fail "ptrace succeeded?: exit code $r"
	fi

	trace "kill agent"
	${SSHAGENT} -k > /dev/null
fi
