#	$OpenBSD: test-exec.sh,v 1.14 2002/04/15 15:19:48 markus Exp $
#	Placed in the Public Domain.

PORT=4242
USER=`id -un`
SUDO=
#SUDO=sudo

OBJ=$1
if [ "x$OBJ" = "x" ]; then
	echo '$OBJ not defined'
	exit 2
fi
if [ ! -d $OBJ ]; then
	echo "not a directory: $OBJ"
	exit 2
fi
SCRIPT=$2
if [ "x$SCRIPT" = "x" ]; then
	echo '$SCRIPT not defined'
	exit 2
fi
if [ ! -f $SCRIPT ]; then
	echo "not a file: $SCRIPT"
	exit 2
fi
if sh -n $SCRIPT; then
	true
else
	echo "syntax error in $SCRIPT"
	exit 2
fi
unset SSH_AUTH_SOCK

# defaults
SSH=ssh
SSHD=sshd
SSHAGENT=ssh-agent
SSHADD=ssh-add
SSHKEYGEN=ssh-keygen
SSHKEYSCAN=ssh-keyscan
SFTP=sftp
SFTPSERVER=/usr/libexec/openssh/sftp-server

if [ "x$TEST_SSH_SSH" != "x" ]; then
	SSH=${TEST_SSH_SSH}
fi
if [ "x$TEST_SSH_SSHD" != "x" ]; then
	SSHD=${TEST_SSH_SSHD}
fi
if [ "x$TEST_SSH_SSHAGENT" != "x" ]; then
	SSHAGENT=${TEST_SSH_SSHAGENT}
fi
if [ "x$TEST_SSH_SSHADD" != "x" ]; then
	SSHADD=${TEST_SSH_SSHADD}
fi
if [ "x$TEST_SSH_SSHKEYGEN" != "x" ]; then
	SSHKEYGEN=${TEST_SSH_SSHKEYGEN}
fi
if [ "x$TEST_SSH_SSHKEYSCAN" != "x" ]; then
	SSHKEYSCAN=${TEST_SSH_SSHKEYSCAN}
fi
if [ "x$TEST_SSH_SFTP" != "x" ]; then
	SFTP=${TEST_SSH_SFTP}
fi
if [ "x$TEST_SSH_SFTPSERVER" != "x" ]; then
	SFTPSERVER=${TEST_SSH_SFTPSERVER}
fi

# these should be used in tests
export SSH SSHD SSHAGENT SSHADD SSHKEYGEN SSHKEYSCAN SFTP SFTPSERVER
#echo $SSH $SSHD $SSHAGENT $SSHADD $SSHKEYGEN $SSHKEYSCAN $SFTP $SFTPSERVER

# helper
cleanup ()
{
	if [ -f $PIDFILE ]; then
		pid=`cat $PIDFILE`
		if [ "X$pid" = "X" ]; then
			echo no sshd running
		else
			if [ $pid -lt 2 ]; then
				echo bad pid for ssd: $pid
			else
				$SUDO kill $pid
			fi
		fi
	fi
}

trace ()
{
	if [ "X$TEST_SSH_TRACE" = "Xyes" ]; then
		echo "$@"
	fi
}

verbose ()
{
	if [ "X$TEST_SSH_QUIET" != "Xyes" ]; then
		echo "$@"
	fi
}


fail ()
{
	RESULT=1
	echo "$@"
}

fatal ()
{
	echo -n "FATAL: "
	fail "$@"
	cleanup
	exit $RESULT
}

RESULT=0
PIDFILE=$OBJ/pidfile

trap fatal 3 2

# create server config
cat << EOF > $OBJ/sshd_config
	Port			$PORT
	ListenAddress		127.0.0.1
	#ListenAddress		::1
	PidFile			$PIDFILE
	AuthorizedKeysFile	$OBJ/authorized_keys_%u
	LogLevel		QUIET
EOF

# server config for proxy connects
cp $OBJ/sshd_config $OBJ/sshd_proxy

# allow group-writable directories in proxy-mode
echo 'StrictModes no' >> $OBJ/sshd_proxy

# create client config
cat << EOF > $OBJ/ssh_config
Host *
	Hostname		127.0.0.1
	HostKeyAlias		localhost-with-alias
	Port			$PORT
	User			$USER
	GlobalKnownHostsFile	$OBJ/known_hosts
	UserKnownHostsFile	$OBJ/known_hosts
	RSAAuthentication	yes
	PubkeyAuthentication	yes
	ChallengeResponseAuthentication	no
	HostbasedAuthentication	no
	PasswordAuthentication	no
	RhostsAuthentication	no
	RhostsRSAAuthentication	no
	BatchMode		yes
	StrictHostKeyChecking	yes
EOF

rm -f $OBJ/known_hosts $OBJ/authorized_keys_$USER

trace "generate keys"
for t in rsa rsa1; do
	# generate user key
	rm -f $OBJ/$t
	${SSHKEYGEN} -q -N '' -t $t  -f $OBJ/$t ||\
		fail "ssh-keygen for $t failed"

	# known hosts file for client
	(
		echo -n 'localhost-with-alias,127.0.0.1,::1 '
		cat $OBJ/$t.pub
	) >> $OBJ/known_hosts

	# setup authorized keys
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
	echo IdentityFile $OBJ/$t >> $OBJ/ssh_config

	# use key as host key, too
	$SUDO cp $OBJ/$t $OBJ/host.$t
	echo HostKey $OBJ/host.$t >> $OBJ/sshd_config

	# don't use SUDO for proxy connect
	echo HostKey $OBJ/$t >> $OBJ/sshd_proxy
done
chmod 644 $OBJ/authorized_keys_$USER

# create a proxy version of the client config
(
	cat $OBJ/ssh_config
	echo proxycommand ${SSHD} -i -f $OBJ/sshd_proxy
) > $OBJ/ssh_proxy

# check proxy config
${SSHD} -t -f $OBJ/sshd_proxy	|| fatal "sshd_proxy broken"

start_sshd ()
{
	# start sshd
	$SUDO ${SSHD} -f $OBJ/sshd_config -t	|| fatal "sshd_config broken"
	$SUDO ${SSHD} -f $OBJ/sshd_config

	trace "wait for sshd"
	i=0;
	while [ ! -f $PIDFILE -a $i -lt 5 ]; do
		i=`expr $i + 1`
		sleep $i
	done

	test -f $PIDFILE || fatal "no sshd running on port $PORT"
}

# source test body
. $SCRIPT

# kill sshd
cleanup
if [ $RESULT -eq 0 ]; then
	verbose ok $tid
else
	echo failed $tid
fi
exit $RESULT
