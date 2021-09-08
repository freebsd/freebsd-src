#	$OpenBSD: reconfigure.sh,v 1.9 2021/06/10 09:46:28 dtucker Exp $
#	Placed in the Public Domain.

tid="simple connect after reconfigure"

# we need the full path to sshd for -HUP
if test "x$USE_VALGRIND" = "x" ; then
	case $SSHD in
	/*)
		# full path is OK
		;;
	*)
		# otherwise make fully qualified
		SSHD=$OBJ/$SSHD
	esac
fi

start_sshd

trace "connect before restart"
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect with failed before reconfigure"
fi

PID=`$SUDO cat $PIDFILE`
rm -f $PIDFILE
$SUDO kill -HUP $PID

trace "wait for sshd to restart"
i=0;
while [ ! -f $PIDFILE -a $i -lt 10 ]; do
	i=`expr $i + 1`
	sleep $i
done

test -f $PIDFILE || fatal "sshd did not restart"

trace "connect after restart"
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect with failed after reconfigure"
fi

trace "reconfigure with active clients"
${SSH} -F $OBJ/ssh_config somehost sleep 10  # authenticated client
${NC} -d 127.0.0.1 $PORT >/dev/null &  # unauthenticated client
PID=`$SUDO cat $PIDFILE`
rm -f $PIDFILE
$SUDO kill -HUP $PID

trace "wait for sshd to restart"
i=0;
while [ ! -f $PIDFILE -a $i -lt 10 ]; do
	i=`expr $i + 1`
	sleep $i
done

test -f $PIDFILE || fatal "sshd did not restart"

trace "connect after restart with active clients"
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect with failed after reconfigure"
fi
