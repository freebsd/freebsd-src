#	$OpenBSD: connection-timeout.sh,v 1.2 2023/01/17 10:15:10 djm Exp $
#	Placed in the Public Domain.

tid="unused connection timeout"
if config_defined DISABLE_FD_PASSING ; then
       skip "not supported on this platform"
fi

CTL=$OBJ/ctl-sock
cp $OBJ/sshd_proxy $OBJ/sshd_proxy.orig

check_ssh() {
	test -S $CTL || return 1
	if ! ${REAL_SSH} -qF$OBJ/ssh_proxy -O check \
	     -oControlPath=$CTL somehost >/dev/null 2>&1 ; then
		return 1
	fi
	return 0
}

start_ssh() {
	trace "start ssh"
	${SSH} -nNfF $OBJ/ssh_proxy "$@" -oExitOnForwardFailure=yes \
	    -oControlMaster=yes -oControlPath=$CTL somehost
	r=$?
	test $r -eq 0 || fatal "failed to start ssh $r"
	check_ssh || fatal "ssh process unresponsive"
}

stop_ssh() {
	test -S $CTL || return
	check_ssh || fatal "ssh process is unresponsive: cannot close"
	if ! ${REAL_SSH} -qF$OBJ/ssh_proxy -O exit \
	     -oControlPath=$CTL >/dev/null somehost >/dev/null ; then
		fatal "ssh process did not respond to close"
	fi
	n=0
	while [ "$n" -lt 20 ] ; do
		test -S $CTL || break
		sleep 1
		n=`expr $n + 1`
	done
	if test -S $CTL ; then
		fatal "ssh process did not exit"
	fi
}

trap "stop_ssh" EXIT

verbose "no timeout"
start_ssh
sleep 5
check_ssh || fatal "ssh unexpectedly missing"
stop_ssh

(cat $OBJ/sshd_proxy.orig ; echo "UnusedConnectionTimeout 2") > $OBJ/sshd_proxy

verbose "timeout"
start_ssh
sleep 8
check_ssh && fail "ssh unexpectedly present"
stop_ssh

verbose "session inhibits timeout"
rm -f $OBJ/copy.1
start_ssh
${REAL_SSH} -qoControlPath=$CTL -oControlMaster=no -Fnone somehost \
	"sleep 8; touch $OBJ/copy.1" &
check_ssh || fail "ssh unexpectedly missing"
wait
test -f $OBJ/copy.1 || fail "missing result file"

verbose "timeout after session"
# Session should still be running from previous
sleep 8
check_ssh && fail "ssh unexpectedly present"
stop_ssh

LPORT=`expr $PORT + 1`
RPORT=`expr $LPORT + 1`
DPORT=`expr $RPORT + 1`
RDPORT=`expr $DPORT + 1`
verbose "timeout with listeners"
start_ssh -L$LPORT:127.0.0.1:$PORT -R$RPORT:127.0.0.1:$PORT -D$DPORT -R$RDPORT
sleep 8
check_ssh && fail "ssh unexpectedly present"
stop_ssh
