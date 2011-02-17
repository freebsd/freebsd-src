#	$OpenBSD: test-exec.sh,v 1.37 2010/02/24 06:21:56 djm Exp $
#	Placed in the Public Domain.

#SUDO=sudo

# Unbreak GNU head(1)
_POSIX2_VERSION=199209
export _POSIX2_VERSION

case `uname -s 2>/dev/null` in
OSF1*)
	BIN_SH=xpg4
	export BIN_SH
	;;
esac

if [ ! -z "$TEST_SSH_PORT" ]; then
	PORT="$TEST_SSH_PORT"
else
	PORT=4242
fi

if [ -x /usr/ucb/whoami ]; then
	USER=`/usr/ucb/whoami`
elif whoami >/dev/null 2>&1; then
	USER=`whoami`
elif logname >/dev/null 2>&1; then
	USER=`logname`
else
	USER=`id -un`
fi

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
if $TEST_SHELL -n $SCRIPT; then
	true
else
	echo "syntax error in $SCRIPT"
	exit 2
fi
unset SSH_AUTH_SOCK

SRC=`dirname ${SCRIPT}`

# defaults
SSH=ssh
SSHD=sshd
SSHAGENT=ssh-agent
SSHADD=ssh-add
SSHKEYGEN=ssh-keygen
SSHKEYSCAN=ssh-keyscan
SFTP=sftp
SFTPSERVER=/usr/libexec/openssh/sftp-server
SCP=scp

# Interop testing
PLINK=plink
PUTTYGEN=puttygen
CONCH=conch

if [ "x$TEST_SSH_SSH" != "x" ]; then
	SSH="${TEST_SSH_SSH}"
fi
if [ "x$TEST_SSH_SSHD" != "x" ]; then
	SSHD="${TEST_SSH_SSHD}"
fi
if [ "x$TEST_SSH_SSHAGENT" != "x" ]; then
	SSHAGENT="${TEST_SSH_SSHAGENT}"
fi
if [ "x$TEST_SSH_SSHADD" != "x" ]; then
	SSHADD="${TEST_SSH_SSHADD}"
fi
if [ "x$TEST_SSH_SSHKEYGEN" != "x" ]; then
	SSHKEYGEN="${TEST_SSH_SSHKEYGEN}"
fi
if [ "x$TEST_SSH_SSHKEYSCAN" != "x" ]; then
	SSHKEYSCAN="${TEST_SSH_SSHKEYSCAN}"
fi
if [ "x$TEST_SSH_SFTP" != "x" ]; then
	SFTP="${TEST_SSH_SFTP}"
fi
if [ "x$TEST_SSH_SFTPSERVER" != "x" ]; then
	SFTPSERVER="${TEST_SSH_SFTPSERVER}"
fi
if [ "x$TEST_SSH_SCP" != "x" ]; then
	SCP="${TEST_SSH_SCP}"
fi
if [ "x$TEST_SSH_PLINK" != "x" ]; then
	# Find real binary, if it exists
	case "${TEST_SSH_PLINK}" in
	/*) PLINK="${TEST_SSH_PLINK}" ;;
	*) PLINK=`which ${TEST_SSH_PLINK} 2>/dev/null` ;;
	esac
fi
if [ "x$TEST_SSH_PUTTYGEN" != "x" ]; then
	# Find real binary, if it exists
	case "${TEST_SSH_PUTTYGEN}" in
	/*) PUTTYGEN="${TEST_SSH_PUTTYGEN}" ;;
	*) PUTTYGEN=`which ${TEST_SSH_PUTTYGEN} 2>/dev/null` ;;
	esac
fi
if [ "x$TEST_SSH_CONCH" != "x" ]; then
	# Find real binary, if it exists
	case "${TEST_SSH_CONCH}" in
	/*) CONCH="${TEST_SSH_CONCH}" ;;
	*) CONCH=`which ${TEST_SSH_CONCH} 2>/dev/null` ;;
	esac
fi

# Path to sshd must be absolute for rexec
case "$SSHD" in
/*) ;;
*) SSHD=`which sshd` ;;
esac

if [ "x$TEST_SSH_LOGFILE" = "x" ]; then
	TEST_SSH_LOGFILE=/dev/null
fi

# these should be used in tests
export SSH SSHD SSHAGENT SSHADD SSHKEYGEN SSHKEYSCAN SFTP SFTPSERVER SCP
#echo $SSH $SSHD $SSHAGENT $SSHADD $SSHKEYGEN $SSHKEYSCAN $SFTP $SFTPSERVER $SCP

# helper
echon()
{
       if [ "x`echo -n`" = "x" ]; then
               echo -n "$@"
       elif [ "x`echo '\c'`" = "x" ]; then
               echo "$@\c"
       else
               fatal "Don't know how to echo without newline."
       fi
}

have_prog()
{
	saved_IFS="$IFS"
	IFS=":"
	for i in $PATH
	do
		if [ -x $i/$1 ]; then
			IFS="$saved_IFS"
			return 0
		fi
	done
	IFS="$saved_IFS"
	return 1
}

cleanup ()
{
	if [ -f $PIDFILE ]; then
		pid=`$SUDO cat $PIDFILE`
		if [ "X$pid" = "X" ]; then
			echo no sshd running
		else
			if [ $pid -lt 2 ]; then
				echo bad pid for ssh: $pid
			else
				$SUDO kill $pid
				trace "wait for sshd to exit"
				i=0;
				while [ -f $PIDFILE -a $i -lt 5 ]; do
					i=`expr $i + 1`
					sleep $i
				done
				test -f $PIDFILE && \
				    fatal "sshd didn't exit port $PORT pid $pid"
			fi
		fi
	fi
}

trace ()
{
	echo "trace: $@" >>$TEST_SSH_LOGFILE
	if [ "X$TEST_SSH_TRACE" = "Xyes" ]; then
		echo "$@"
	fi
}

verbose ()
{
	echo "verbose: $@" >>$TEST_SSH_LOGFILE
	if [ "X$TEST_SSH_QUIET" != "Xyes" ]; then
		echo "$@"
	fi
}


fail ()
{
	echo "FAIL: $@" >>$TEST_SSH_LOGFILE
	RESULT=1
	echo "$@"
}

fatal ()
{
	echo "FATAL: $@" >>$TEST_SSH_LOGFILE
	echon "FATAL: "
	fail "$@"
	cleanup
	exit $RESULT
}

# Check whether preprocessor symbols are defined in config.h.
config_defined ()
{
	str=$1
	while test "x$2" != "x" ; do
		str="$str|$2"
		shift
	done
	egrep "^#define.*($str)" ${BUILDDIR}/config.h >/dev/null 2>&1
}

RESULT=0
PIDFILE=$OBJ/pidfile

trap fatal 3 2

# create server config
cat << EOF > $OBJ/sshd_config
	StrictModes		no
	Port			$PORT
	Protocol		2,1
	AddressFamily		inet
	ListenAddress		127.0.0.1
	#ListenAddress		::1
	PidFile			$PIDFILE
	AuthorizedKeysFile	$OBJ/authorized_keys_%u
	LogLevel		VERBOSE
	AcceptEnv		_XXX_TEST_*
	AcceptEnv		_XXX_TEST
	Subsystem	sftp	$SFTPSERVER
EOF

if [ ! -z "$TEST_SSH_SSHD_CONFOPTS" ]; then
	trace "adding sshd_config option $TEST_SSH_SSHD_CONFOPTS"
	echo "$TEST_SSH_SSHD_CONFOPTS" >> $OBJ/sshd_config
fi

# server config for proxy connects
cp $OBJ/sshd_config $OBJ/sshd_proxy

# allow group-writable directories in proxy-mode
echo 'StrictModes no' >> $OBJ/sshd_proxy

# create client config
cat << EOF > $OBJ/ssh_config
Host *
	Protocol		2,1
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
	BatchMode		yes
	StrictHostKeyChecking	yes
EOF

if [ ! -z "$TEST_SSH_SSH_CONFOPTS" ]; then
	trace "adding ssh_config option $TEST_SSH_SSHD_CONFOPTS"
	echo "$TEST_SSH_SSH_CONFOPTS" >> $OBJ/ssh_config
fi

rm -f $OBJ/known_hosts $OBJ/authorized_keys_$USER

trace "generate keys"
for t in rsa rsa1; do
	# generate user key
	rm -f $OBJ/$t
	${SSHKEYGEN} -b 1024 -q -N '' -t $t  -f $OBJ/$t ||\
		fail "ssh-keygen for $t failed"

	# known hosts file for client
	(
		echon 'localhost-with-alias,127.0.0.1,::1 '
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

# Activate Twisted Conch tests if the binary is present
REGRESS_INTEROP_CONCH=no
if test -x "$CONCH" ; then
	REGRESS_INTEROP_CONCH=yes
fi

# If PuTTY is present and we are running a PuTTY test, prepare keys and
# configuration
REGRESS_INTEROP_PUTTY=no
if test -x "$PUTTYGEN" -a -x "$PLINK" ; then
	REGRESS_INTEROP_PUTTY=yes
fi
case "$SCRIPT" in
*putty*)	;;
*)		REGRESS_INTEROP_PUTTY=no ;;
esac

if test "$REGRESS_INTEROP_PUTTY" = "yes" ; then
	mkdir -p ${OBJ}/.putty

	# Add a PuTTY key to authorized_keys
	rm -f ${OBJ}/putty.rsa2
	puttygen -t rsa -o ${OBJ}/putty.rsa2 < /dev/null > /dev/null
	puttygen -O public-openssh ${OBJ}/putty.rsa2 \
	    >> $OBJ/authorized_keys_$USER

	# Convert rsa2 host key to PuTTY format
	${SRC}/ssh2putty.sh 127.0.0.1 $PORT $OBJ/rsa > \
	    ${OBJ}/.putty/sshhostkeys
	${SRC}/ssh2putty.sh 127.0.0.1 22 $OBJ/rsa >> \
	    ${OBJ}/.putty/sshhostkeys

	# Setup proxied session
	mkdir -p ${OBJ}/.putty/sessions
	rm -f ${OBJ}/.putty/sessions/localhost_proxy
	echo "Hostname=127.0.0.1" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "PortNumber=$PORT" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "ProxyMethod=5" >> ${OBJ}/.putty/sessions/localhost_proxy
	echo "ProxyTelnetCommand=sh ${SRC}/sshd-log-wrapper.sh ${SSHD} ${TEST_SSH_LOGFILE} -i -f $OBJ/sshd_proxy" >> ${OBJ}/.putty/sessions/localhost_proxy 

	REGRESS_INTEROP_PUTTY=yes
fi

# create a proxy version of the client config
(
	cat $OBJ/ssh_config
	echo proxycommand ${SUDO} sh ${SRC}/sshd-log-wrapper.sh ${SSHD} ${TEST_SSH_LOGFILE} -i -f $OBJ/sshd_proxy
) > $OBJ/ssh_proxy

# check proxy config
${SSHD} -t -f $OBJ/sshd_proxy	|| fatal "sshd_proxy broken"

start_sshd ()
{
	# start sshd
	$SUDO ${SSHD} -f $OBJ/sshd_config "$@" -t || fatal "sshd_config broken"
	$SUDO ${SSHD} -f $OBJ/sshd_config -e "$@" >>$TEST_SSH_LOGFILE 2>&1

	trace "wait for sshd"
	i=0;
	while [ ! -f $PIDFILE -a $i -lt 10 ]; do
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
