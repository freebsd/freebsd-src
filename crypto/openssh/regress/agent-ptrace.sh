#	$OpenBSD: agent-ptrace.sh,v 1.5 2022/04/22 05:08:43 anton Exp $
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

if [ "x$USER" = "xroot" ]; then
	echo "Skipped: running as root"
	exit 0
fi

if have_prog gdb ; then
	: ok
else
	echo "skipped (gdb not found)"
	exit 0
fi

if $OBJ/setuid-allowed ${SSHAGENT} ; then
	: ok
else
	echo "skipped (${SSHAGENT} is mounted on a no-setuid filesystem)"
	exit 0
fi

if test -z "$SUDO" ; then
	echo "skipped (SUDO not set)"
	exit 0
else
	$SUDO chown 0 ${SSHAGENT}
	$SUDO chgrp 0 ${SSHAGENT}
	$SUDO chmod 2755 ${SSHAGENT}
	trap "$SUDO chown ${USER} ${SSHAGENT}; $SUDO chmod 755 ${SSHAGENT}" 0
fi

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	# ls -l ${SSH_AUTH_SOCK}
	gdb ${SSHAGENT} ${SSH_AGENT_PID} > ${OBJ}/gdb.out 2>&1 << EOF
		quit
EOF
	r=$?
	if [ $r -ne 0 ]; then
		fail "gdb failed: exit code $r"
	fi
	egrep 'Operation not permitted.|Permission denied.|Invalid argument.|Unable to access task|Inappropriate ioctl for device.' >/dev/null ${OBJ}/gdb.out
	r=$?
	rm -f ${OBJ}/gdb.out
	if [ $r -ne 0 ]; then
		fail "ptrace succeeded?: exit code $r"
	fi

	trace "kill agent"
	${SSHAGENT} -k > /dev/null
fi
