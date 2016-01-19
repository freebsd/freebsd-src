#	$OpenBSD: cfgmatch.sh,v 1.9 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="sshd_config match"

pidfile=$OBJ/remote_pid
fwdport=3301
fwd="-L $fwdport:127.0.0.1:$PORT"

echo "ExitOnForwardFailure=yes" >> $OBJ/ssh_config
echo "ExitOnForwardFailure=yes" >> $OBJ/ssh_proxy

start_client()
{
	rm -f $pidfile
	${SSH} -q -$p $fwd "$@" somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' \
	    >>$TEST_REGRESS_LOGFILE 2>&1 &
	client_pid=$!
	# Wait for remote end
	n=0
	while test ! -f $pidfile ; do
		sleep 1
		n=`expr $n + 1`
		if test $n -gt 60; then
			kill $client_pid
			fatal "timeout waiting for background ssh"
		fi
	done	
}

stop_client()
{
	pid=`cat $pidfile`
	if [ ! -z "$pid" ]; then
		kill $pid
	fi
	wait
}

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak
echo "PermitOpen 127.0.0.1:1" >>$OBJ/sshd_config
echo "Match Address 127.0.0.1" >>$OBJ/sshd_config
echo "PermitOpen 127.0.0.1:$PORT" >>$OBJ/sshd_config

grep -v AuthorizedKeysFile $OBJ/sshd_proxy_bak > $OBJ/sshd_proxy
echo "AuthorizedKeysFile /dev/null" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1" >>$OBJ/sshd_proxy
echo "Match user $USER" >>$OBJ/sshd_proxy
echo "AuthorizedKeysFile /dev/null $OBJ/authorized_keys_%u" >>$OBJ/sshd_proxy
echo "Match Address 127.0.0.1" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:$PORT" >>$OBJ/sshd_proxy

start_sshd

#set -x

# Test Match + PermitOpen in sshd_config.  This should be permitted
for p in ${SSH_PROTOCOLS}; do
	trace "match permitopen localhost proto $p"
	start_client -F $OBJ/ssh_config
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true || \
	    fail "match permitopen permit proto $p"
	stop_client
done

# Same but from different source.  This should not be permitted
for p in ${SSH_PROTOCOLS}; do
	trace "match permitopen proxy proto $p"
	start_client -F $OBJ/ssh_proxy
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true && \
	    fail "match permitopen deny proto $p"
	stop_client
done

# Retry previous with key option, should also be denied.
cp /dev/null $OBJ/authorized_keys_$USER
for t in ${SSH_KEYTYPES}; do
	printf 'permitopen="127.0.0.1:'$PORT'" ' >> $OBJ/authorized_keys_$USER
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
done
for p in ${SSH_PROTOCOLS}; do
	trace "match permitopen proxy w/key opts proto $p"
	start_client -F $OBJ/ssh_proxy
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true && \
	    fail "match permitopen deny w/key opt proto $p"
	stop_client
done

# Test both sshd_config and key options permitting the same dst/port pair.
# Should be permitted.
for p in ${SSH_PROTOCOLS}; do
	trace "match permitopen localhost proto $p"
	start_client -F $OBJ/ssh_config
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true || \
	    fail "match permitopen permit proto $p"
	stop_client
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:$PORT 127.0.0.2:2" >>$OBJ/sshd_proxy
echo "Match User $USER" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:2" >>$OBJ/sshd_proxy

# Test that a Match overrides a PermitOpen in the global section
for p in ${SSH_PROTOCOLS}; do
	trace "match permitopen proxy w/key opts proto $p"
	start_client -F $OBJ/ssh_proxy
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true && \
	    fail "match override permitopen proto $p"
	stop_client
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:$PORT 127.0.0.2:2" >>$OBJ/sshd_proxy
echo "Match User NoSuchUser" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:2" >>$OBJ/sshd_proxy

# Test that a rule that doesn't match doesn't override, plus test a
# PermitOpen entry that's not at the start of the list
for p in ${SSH_PROTOCOLS}; do
	trace "nomatch permitopen proxy w/key opts proto $p"
	start_client -F $OBJ/ssh_proxy
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true || \
	    fail "nomatch override permitopen proto $p"
	stop_client
done
