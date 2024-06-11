#	$OpenBSD: channel-timeout.sh,v 1.2 2024/01/09 22:19:36 djm Exp $
#	Placed in the Public Domain.

tid="channel timeout"

# XXX not comprehensive. Still need -R -L agent X11 forwarding + interactive

rm -f $OBJ/finished.* $OBJ/mux.*

MUXPATH=$OBJ/mux.$$
open_mux() {
	${SSH} -nNfM -oControlPath=$MUXPATH -F $OBJ/ssh_proxy "$@" somehost ||
	    fatal "open mux failed"
	test -e $MUXPATH || fatal "mux socket $MUXPATH not established"
}

close_mux() {
	test -e $MUXPATH || fatal "mux socket $MUXPATH missing"
	${SSH} -qF $OBJ/ssh_proxy -oControlPath=$MUXPATH -O exit somehost ||
	    fatal "could not terminate mux process"
	for x in 1 2 3 4 5 6 7 8 9 10 ; do
		test -e $OBJ/mux && break
		sleep 1
	done
	test -e $MUXPATH && fatal "mux did not clean up"
}
mux_client() {
	${SSH} -F $OBJ/ssh_proxy -oControlPath=$MUXPATH somehost "$@"
}

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

verbose "command long timeout"
(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout session:command=60") \
	> $OBJ/sshd_proxy
${SSH} -F $OBJ/ssh_proxy somehost "exit 23"
r=$?
if [ $r -ne 23 ]; then
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

if config_defined DISABLE_FD_PASSING ; then
	verbose "skipping multiplexing tests"
else
	verbose "multiplexed command timeout"
	(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout session:command=1") \
		> $OBJ/sshd_proxy
	open_mux
	mux_client "sleep 5 ; exit 23"
	r=$?
	if [ $r -ne 255 ]; then
		fail "ssh returned unexpected error code $r"
	fi
	close_mux

	verbose "irrelevant multiplexed command timeout"
	(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout session:shell=1") \
		> $OBJ/sshd_proxy
	open_mux
	mux_client "sleep 5 ; exit 23"
	r=$?
	if [ $r -ne 23 ]; then
		fail "ssh returned unexpected error code $r"
	fi
	close_mux

	verbose "global command timeout"
	(cat $OBJ/sshd_proxy.orig ; echo "ChannelTimeout global=10") \
		> $OBJ/sshd_proxy
	open_mux
	mux_client "sleep 1 ; echo ok ; sleep 1; echo ok; sleep 60; touch $OBJ/finished.1" >/dev/null &
	mux_client "sleep 60 ; touch $OBJ/finished.2" >/dev/null &
	mux_client "sleep 2 ; touch $OBJ/finished.3" >/dev/null &
	wait
	test -f $OBJ/finished.1 && fail "first mux process completed"
	test -f $OBJ/finished.2 && fail "second mux process completed"
	test -f $OBJ/finished.3 || fail "third mux process did not complete"
	close_mux
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
