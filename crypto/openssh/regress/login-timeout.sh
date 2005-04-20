#	$OpenBSD: login-timeout.sh,v 1.1 2004/02/17 08:23:20 dtucker Exp $
#	Placed in the Public Domain.

tid="connect after login grace timeout"

trace "test login grace with privsep"
echo "LoginGraceTime 10s" >> $OBJ/sshd_config
echo "MaxStartups 1" >> $OBJ/sshd_config
start_sshd

(echo SSH-2.0-fake; sleep 60) | telnet localhost ${PORT} >/dev/null 2>&1 & 
sleep 15
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect after login grace timeout failed with privsep"
fi

kill `cat $PIDFILE`

trace "test login grace without privsep"
echo "UsePrivilegeSeparation no" >> $OBJ/sshd_config
start_sshd

(echo SSH-2.0-fake; sleep 60) | telnet localhost ${PORT} >/dev/null 2>&1 & 
sleep 15
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect after login grace timeout failed without privsep"
fi
