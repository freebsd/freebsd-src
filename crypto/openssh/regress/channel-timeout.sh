#	$OpenBSD: channel-timeout.sh,v 1.1 2023/01/06 08:07:39 djm Exp $
#	Placed in the Public Domain.

tid="channel timeout"

# XXX not comprehensive. Still need -R -L agent X11 forwarding + interactive

rm -f $OBJ/sshd_proxy.orig 
cp $OBJ/sshd_proxy $OBJ/sshd_proxy.orig

verbose "no timeout"
${SSH} -F $OBJ/ssh_proxy somehost "sleep 5 ; exit 23"
r=$?
if [ $r -ne 23 ]; then
	fail "ssh failed"
fi

verbose "command timeout"
(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout session:command=1") \
	> $OBJ/sshd_proxy
${SSH} -F $OBJ/ssh_proxy somehost "sleep 5 ; exit 23"
r=$?
if [ $r -ne 255 ]; then
	fail "ssh returned unexpected error code $r"
fi

verbose "command wildcard timeout"
(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout session:*=1") \
	> $OBJ/sshd_proxy
${SSH} -F $OBJ/ssh_proxy somehost "sleep 5 ; exit 23"
r=$?
if [ $r -ne 255 ]; then
	fail "ssh returned unexpected error code $r"
fi

verbose "command irrelevant timeout"
(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout session:shell=1") \
	> $OBJ/sshd_proxy
${SSH} -F $OBJ/ssh_proxy somehost "sleep 5 ; exit 23"
r=$?
if [ $r -ne 23 ]; then
	fail "ssh failed"
fi

# Set up a "slow sftp server" that sleeps before executing the real one.
cat > $OBJ/slow-sftp-server.sh << _EOF
#!/bin/sh

sleep 5
$SFTPSERVER
_EOF
chmod a+x $OBJ/slow-sftp-server.sh

verbose "sftp no timeout"
(grep -vi subsystem.*sftp $OBJ/sshd_proxy.orig;
 echo "Subsystem sftp $OBJ/slow-sftp-server.sh" ) > $OBJ/sshd_proxy

rm -f ${COPY}
$SFTP -qS $SSH -F $OBJ/ssh_proxy somehost:$DATA $COPY
r=$?
if [ $r -ne 0 ]; then
	fail "sftp failed"
fi
cmp $DATA $COPY || fail "corrupted copy"

verbose "sftp timeout"
(grep -vi subsystem.*sftp $OBJ/sshd_proxy.orig;
 echo "ChannelTimeout session:subsystem:sftp=1" ;
 echo "Subsystem sftp $OBJ/slow-sftp-server.sh" ) > $OBJ/sshd_proxy

rm -f ${COPY}
$SFTP -qS $SSH -F $OBJ/ssh_proxy somehost:$DATA $COPY
r=$?
if [ $r -eq 0 ]; then
	fail "sftp succeeded unexpectedly"
fi
test -f $COPY && cmp $DATA $COPY && fail "intact copy"

verbose "sftp irrelevant timeout"
(grep -vi subsystem.*sftp $OBJ/sshd_proxy.orig;
 echo "ChannelTimeout session:subsystem:command=1" ;
 echo "Subsystem sftp $OBJ/slow-sftp-server.sh" ) > $OBJ/sshd_proxy

rm -f ${COPY}
$SFTP -qS $SSH -F $OBJ/ssh_proxy somehost:$DATA $COPY
r=$?
if [ $r -ne 0 ]; then
	fail "sftp failed"
fi
cmp $DATA $COPY || fail "corrupted copy"

