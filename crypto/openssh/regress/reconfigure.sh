#	$OpenBSD: reconfigure.sh,v 1.2 2003/06/21 09:14:05 markus Exp $
#	Placed in the Public Domain.

tid="simple connect after reconfigure"

# we need the full path to sshd for -HUP
case $SSHD in
/*)
	# full path is OK 
	;;
*)
	# otherwise make fully qualified
	SSHD=$OBJ/$SSHD
esac

start_sshd

$SUDO kill -HUP `cat $PIDFILE`
sleep 1

trace "wait for sshd to restart"
i=0;
while [ ! -f $PIDFILE -a $i -lt 10 ]; do
	i=`expr $i + 1`
	sleep $i
done

test -f $PIDFILE || fatal "sshd did not restart"

for p in 1 2; do
	${SSH} -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed after reconfigure"
	fi
done
