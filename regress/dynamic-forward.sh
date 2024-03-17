#	$OpenBSD: dynamic-forward.sh,v 1.17 2024/03/08 11:34:10 dtucker Exp $
#	Placed in the Public Domain.

tid="dynamic forwarding"

# This is a reasonable proxy for IPv6 support.
if ! config_defined HAVE_STRUCT_IN6_ADDR ; then
	SKIP_IPV6=yes
fi

FWDPORT=`expr $PORT + 1`
make_tmpdir
CTL=${SSH_REGRESS_TMP}/ctl-sock
cp $OBJ/ssh_config $OBJ/ssh_config.orig
proxycmd="$OBJ/netcat -x 127.0.0.1:$FWDPORT -X"
trace "will use ProxyCommand $proxycmd"

start_ssh() {
	direction="$1"
	arg="$2"
	n=0
	error="1"
	# Use a multiplexed ssh so we can control its lifecycle.
	trace "start dynamic -$direction forwarding, fork to background"
	(cat $OBJ/ssh_config.orig ; echo "$arg") > $OBJ/ssh_config
	${REAL_SSH} -vvvnNfF $OBJ/ssh_config -E$TEST_SSH_LOGFILE \
	    -$direction $FWDPORT -oExitOnForwardFailure=yes \
	    -oControlMaster=yes -oControlPath=$CTL somehost
	r=$?
	test $r -eq 0 || fatal "failed to start dynamic forwarding $r"
	if ! ${REAL_SSH} -qF$OBJ/ssh_config -O check \
	     -oControlPath=$CTL somehost >/dev/null 2>&1 ; then
		fatal "forwarding ssh process unresponsive"
	fi
}

stop_ssh() {
	test -S $CTL || return
	if ! ${REAL_SSH} -qF$OBJ/ssh_config -O exit \
	     -oControlPath=$CTL >/dev/null somehost >/dev/null ; then
		fatal "forwarding ssh process did not respond to close"
	fi
	n=0
	while [ "$n" -lt 20 ] ; do
		test -S $CTL || break
		sleep 1
		n=`expr $n + 1`
	done
	if test -S $CTL ; then
		fatal "forwarding ssh process did not exit"
	fi
}

check_socks() {
	direction=$1
	expect_success=$2
	for s in 4 5; do
	    for h in 127.0.0.1 localhost; do
		trace "testing ssh socks version $s host $h (-$direction)"
		${REAL_SSH} -q -F $OBJ/ssh_config -o \
		   "ProxyCommand ${TEST_SHELL} -c '${proxycmd}${s} $h $PORT 2>/dev/null'" \
		   somehost cat ${DATA} > ${COPY}
		r=$?
		if [ "x$expect_success" = "xY" ] ; then
			if [ $r -ne 0 ] ; then
				fail "ssh failed with exit status $r"
			fi
			test -f ${COPY}	 || fail "failed copy ${DATA}"
			cmp ${DATA} ${COPY} || fail "corrupted copy of ${DATA}"
		elif [ $r -eq 0 ] ; then
			fail "ssh unexpectedly succeeded"
		fi
	    done
	done
}

start_sshd
trap "stop_ssh" EXIT

for d in D R; do
	verbose "test -$d forwarding"
	start_ssh $d
	check_socks $d Y
	stop_ssh
	test "x$d" = "xR" || continue
	
	# Test PermitRemoteOpen
	verbose "PermitRemoteOpen=any"
	start_ssh $d PermitRemoteOpen=any
	check_socks $d Y
	stop_ssh

	verbose "PermitRemoteOpen=none"
	start_ssh $d PermitRemoteOpen=none
	check_socks $d N
	stop_ssh

	verbose "PermitRemoteOpen=explicit"
	permit="127.0.0.1:$PORT [::1]:$PORT localhost:$PORT"
	test -z "$SKIP_IPV6" || permit="127.0.0.1:$PORT localhost:$PORT"
	start_ssh $d PermitRemoteOpen="$permit"
	check_socks $d Y
	stop_ssh

	verbose "PermitRemoteOpen=disallowed"
	permit="127.0.0.1:1 [::1]:1 localhost:1"
	test -z "$SKIP_IPV6" || permit="127.0.0.1:1 localhost:1"
	start_ssh $d PermitRemoteOpen="$permit"
	check_socks $d N
	stop_ssh
done
