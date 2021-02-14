#	$OpenBSD: cfgmatch.sh,v 1.12 2019/04/18 18:57:16 dtucker Exp $
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
	${SSH} -q $fwd "$@" somehost \
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
echo "PermitOpen 127.0.0.1:2 127.0.0.1:3 127.0.0.1:$PORT" >>$OBJ/sshd_config

grep -v AuthorizedKeysFile $OBJ/sshd_proxy_bak > $OBJ/sshd_proxy
echo "AuthorizedKeysFile /dev/null" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1" >>$OBJ/sshd_proxy
echo "Match user $USER" >>$OBJ/sshd_proxy
echo "AuthorizedKeysFile /dev/null $OBJ/authorized_keys_%u" >>$OBJ/sshd_proxy
echo "Match Address 127.0.0.1" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:2 127.0.0.1:3 127.0.0.1:$PORT" >>$OBJ/sshd_proxy

${SUDO} ${SSHD} -f $OBJ/sshd_config -T >/dev/null || \
    fail "config w/match fails config test"

start_sshd

# Test Match + PermitOpen in sshd_config.  This should be permitted
trace "match permitopen localhost"
start_client -F $OBJ/ssh_config
${SSH} -q -p $fwdport -F $OBJ/ssh_config somehost true || \
    fail "match permitopen permit"
stop_client

# Same but from different source.  This should not be permitted
trace "match permitopen proxy"
start_client -F $OBJ/ssh_proxy
${SSH} -q -p $fwdport -F $OBJ/ssh_config somehost true && \
    fail "match permitopen deny"
stop_client

# Retry previous with key option, should also be denied.
cp /dev/null $OBJ/authorized_keys_$USER
for t in ${SSH_KEYTYPES}; do
	printf 'permitopen="127.0.0.1:'$PORT'" ' >> $OBJ/authorized_keys_$USER
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
done
trace "match permitopen proxy w/key opts"
start_client -F $OBJ/ssh_proxy
${SSH} -q -p $fwdport -F $OBJ/ssh_config somehost true && \
    fail "match permitopen deny w/key opt"
stop_client

# Test both sshd_config and key options permitting the same dst/port pair.
# Should be permitted.
trace "match permitopen localhost"
start_client -F $OBJ/ssh_config
${SSH} -q -p $fwdport -F $OBJ/ssh_config somehost true || \
    fail "match permitopen permit"
stop_client

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:$PORT 127.0.0.2:2" >>$OBJ/sshd_proxy
echo "Match User $USER" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:2" >>$OBJ/sshd_proxy

# Test that a Match overrides a PermitOpen in the global section
trace "match permitopen proxy w/key opts"
start_client -F $OBJ/ssh_proxy
${SSH} -q -p $fwdport -F $OBJ/ssh_config somehost true && \
    fail "match override permitopen"
stop_client

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:$PORT 127.0.0.2:2" >>$OBJ/sshd_proxy
echo "Match User NoSuchUser" >>$OBJ/sshd_proxy
echo "PermitOpen 127.0.0.1:1 127.0.0.1:2" >>$OBJ/sshd_proxy

# Test that a rule that doesn't match doesn't override, plus test a
# PermitOpen entry that's not at the start of the list
trace "nomatch permitopen proxy w/key opts"
start_client -F $OBJ/ssh_proxy
${SSH} -q -p $fwdport -F $OBJ/ssh_config somehost true || \
    fail "nomatch override permitopen"
stop_client

# Test parsing of available Match criteria (with the exception of Group which
# requires knowledge of actual group memberships user running the test).
params="user:user:u1 host:host:h1 address:addr:1.2.3.4 \
    localaddress:laddr:5.6.7.8 rdomain:rdomain:rdom1"
cp $OBJ/sshd_proxy_bak $OBJ/sshd_config
echo 'Banner /nomatch' >>$OBJ/sshd_config
for i in $params; do
	config=`echo $i | cut -f1 -d:`
	criteria=`echo $i | cut -f2 -d:`
	value=`echo $i | cut -f3 -d:`
	cat >>$OBJ/sshd_config <<EOD
	    Match $config $value
	      Banner /$value
EOD
done

${SUDO} ${SSHD} -f $OBJ/sshd_config -T >/dev/null || \
    fail "validate config for w/out spec"

# Test matching each criteria.
for i in $params; do
	testcriteria=`echo $i | cut -f2 -d:`
	expected=/`echo $i | cut -f3 -d:`
	spec=""
	for j in $params; do
		config=`echo $j | cut -f1 -d:`
		criteria=`echo $j | cut -f2 -d:`
		value=`echo $j | cut -f3 -d:`
		if [ "$criteria" = "$testcriteria" ]; then
			spec="$criteria=$value,$spec"
		else
			spec="$criteria=1$value,$spec"
		fi
	done
	trace "test spec $spec"
	result=`${SUDO} ${SSHD} -f $OBJ/sshd_config -T -C "$spec" | \
	    awk '$1=="banner"{print $2}'`
	if [ "$result" != "$expected" ]; then
		fail "match $config expected $expected got $result"
	fi
done
