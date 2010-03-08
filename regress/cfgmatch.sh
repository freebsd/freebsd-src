#	$OpenBSD: cfgmatch.sh,v 1.4 2006/12/13 08:36:36 dtucker Exp $
#	Placed in the Public Domain.

tid="sshd_config match"

pidfile=$OBJ/remote_pid
fwdport=3301
fwd="-L $fwdport:127.0.0.1:$PORT"

stop_client()
{
	pid=`cat $pidfile`
	if [ ! -z "$pid" ]; then
		kill $pid
		sleep 1
	fi
}

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

echo "PermitOpen 127.0.0.1:1" >>$OBJ/sshd_config
echo "Match Address 127.0.0.1" >>$OBJ/sshd_config
echo "PermitOpen 127.0.0.1:$PORT" >>$OBJ/sshd_config

echo "PermitOpen 127.0.0.1:1" >>$OBJ/sshd_proxy
echo "Match Address 127.0.0.1" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:$PORT" >>$OBJ/sshd_proxy

start_sshd

#set -x

# Test Match + PermitOpen in sshd_config.  This should be permitted
for p in 1 2; do
	rm -f $pidfile
	trace "match permitopen localhost proto $p"
	${SSH} -$p $fwd -F $OBJ/ssh_config -f somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' >>$TEST_SSH_LOGFILE 2>&1 ||\
	    fail "match permitopen proto $p sshd failed"
	sleep 1;
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true || \
	    fail "match permitopen permit proto $p"
	stop_client
done

# Same but from different source.  This should not be permitted
for p in 1 2; do
	rm -f $pidfile
	trace "match permitopen proxy proto $p"
	${SSH} -q -$p $fwd -F $OBJ/ssh_proxy -f somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' >>$TEST_SSH_LOGFILE 2>&1 ||\
	    fail "match permitopen proxy proto $p sshd failed"
	sleep 1;
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true && \
	    fail "match permitopen deny proto $p"
	stop_client
done

# Retry previous with key option, should also be denied.
echon 'permitopen="127.0.0.1:'$PORT'" ' >$OBJ/authorized_keys_$USER
cat $OBJ/rsa.pub >> $OBJ/authorized_keys_$USER
echon 'permitopen="127.0.0.1:'$PORT'" ' >>$OBJ/authorized_keys_$USER
cat $OBJ/rsa1.pub >> $OBJ/authorized_keys_$USER
for p in 1 2; do
	rm -f $pidfile
	trace "match permitopen proxy w/key opts proto $p"
	${SSH} -q -$p $fwd -F $OBJ/ssh_proxy -f somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' >>$TEST_SSH_LOGFILE 2>&1 ||\
	    fail "match permitopen w/key opt proto $p sshd failed"
	sleep 1;
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true && \
	    fail "match permitopen deny w/key opt proto $p"
	stop_client
done

# Test both sshd_config and key options permitting the same dst/port pair.
# Should be permitted.
for p in 1 2; do
	rm -f $pidfile
	trace "match permitopen localhost proto $p"
	${SSH} -$p $fwd -F $OBJ/ssh_config -f somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' >>$TEST_SSH_LOGFILE 2>&1 ||\
	    fail "match permitopen proto $p sshd failed"
	sleep 1;
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true || \
	    fail "match permitopen permit proto $p"
	stop_client
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:$PORT 127.0.0.2:2" >>$OBJ/sshd_proxy
echo "Match User $USER" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:2" >>$OBJ/sshd_proxy

# Test that a Match overrides a PermitOpen in the global section
for p in 1 2; do
	rm -f $pidfile
	trace "match permitopen proxy w/key opts proto $p"
	${SSH} -q -$p $fwd -F $OBJ/ssh_proxy -f somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' >>$TEST_SSH_LOGFILE 2>&1 ||\
	    fail "match override permitopen proto $p sshd failed"
	sleep 1;
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
for p in 1 2; do
	rm -f $pidfile
	trace "nomatch permitopen proxy w/key opts proto $p"
	${SSH} -q -$p $fwd -F $OBJ/ssh_proxy -f somehost \
	    exec sh -c \'"echo \$\$ > $pidfile; exec sleep 100"\' >>$TEST_SSH_LOGFILE 2>&1 ||\
	    fail "nomatch override permitopen proto $p sshd failed"
	sleep 1;
	${SSH} -q -$p -p $fwdport -F $OBJ/ssh_config somehost true || \
	    fail "nomatch override permitopen proto $p"
	stop_client
done
