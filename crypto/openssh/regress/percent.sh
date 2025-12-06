#	$OpenBSD: percent.sh,v 1.21 2025/04/08 23:10:46 djm Exp $
#	Placed in the Public Domain.

tid="percent expansions"

USER=`id -u -n`
USERID=`id -u`
HOST=`hostname | cut -f1 -d.`
HOSTNAME=`hostname`
HASH=""

# Localcommand is evaluated after connection because %T is not available
# until then.  Because of this we use a different method of exercising it,
# and we can't override the remote user otherwise authentication will fail.
# We also have to explicitly enable it.
echo "permitlocalcommand yes" >> $OBJ/ssh_proxy

trial()
{
	opt="$1"; arg="$2"
	expect=`echo "$3" | sed 's|^//|/|'` # approximate realpath

	trace "test $opt=$arg $expect"
	rm -f $OBJ/actual
	got=""
	case "$opt" in
	localcommand)
		${SSH} -F $OBJ/ssh_proxy -o $opt="echo '$arg' >$OBJ/actual" \
		    somehost true
		got=`cat $OBJ/actual`
		;;
	user|user-l|user-at)
		if [ "$arg" = '%r' ] || [ "$arg" = '%C' ]; then
			# User does not support %r, ie itself or %C.  Skip test.
			got="$expect"
		elif [ "$i" = "user" ]; then
			got=`${SSH} -F $OBJ/ssh_proxy -o $opt="$arg" -G \
			    remuser@somehost | awk '$1=="'$opt'"{print $2}'`
		elif [ "$i" = "user-l" ]; then
			# Also test ssh -l
			got=`${SSH} -F $OBJ/ssh_proxy -l "$arg" -G \
			    somehost | awk '$1=="'user'"{print $2}'`
		elif [ "$i" = "user-at" ]; then
			# Also test user@host
			got=`${SSH} -F $OBJ/ssh_proxy -G "$arg@somehost" | \
			    awk '$1=="'user'"{print $2}'`
		fi
		;;
	userknownhostsfile)
		# Move the userknownhosts file to what the expansion says,
		# make sure ssh works then put it back.
		mv "$OBJ/known_hosts" "$OBJ/$expect"
		${SSH} -F $OBJ/ssh_proxy -o $opt="$OBJ/$arg" somehost true && \
			got="$expect"
		mv "$OBJ/$expect" "$OBJ/known_hosts"
		;;
	matchexec)
		(cat $OBJ/ssh_proxy && \
		 echo "Match Exec \"echo '$arg' >$OBJ/actual\"") \
		    >$OBJ/ssh_proxy_match
		${SSH} -F $OBJ/ssh_proxy_match remuser@somehost true || true
		got=`cat $OBJ/actual`
		;;
	*forward)
		# LocalForward and RemoteForward take two args and only
		# operate on Unix domain socket paths
		got=`${SSH} -F $OBJ/ssh_proxy -o $opt="/$arg /$arg" -G \
		    remuser@somehost | awk '$1=="'$opt'"{print $2" "$3}'`
		expect="/$expect /$expect"
		;;
	setenv)
		# First make sure we don't expand variable names.
		got=`${SSH} -F $OBJ/ssh_proxy -o $opt="$arg=TESTVAL" -G \
		    remuser@somehost | awk '$1=="'$opt'"{print $2}'`
		if [ "$got" != "$arg=TESTVAL" ]; then
			fatal "incorrectly expanded setenv variable name"
		fi
		# Now check that the value expands as expected.
		got=`${SSH} -F $OBJ/ssh_proxy -o $opt=TESTVAL="$arg" -G \
		    remuser@somehost | awk '$1=="'$opt'"{print $2}'`
		got=`echo "$got" | sed 's/^TESTVAL=//'`
		;;
	*)
		got=`${SSH} -F $OBJ/ssh_proxy -o $opt="$arg" -G \
		    remuser@somehost | awk '$1=="'$opt'"{print $2}'`
	esac
	if [ "$got" != "$expect" ]; then
		fail "$opt=$arg expect $expect got $got"
	fi
}

for i in matchexec localcommand remotecommand controlpath identityagent \
    forwardagent localforward remoteforward revokedhostkeys \
    user user-l user-at setenv userknownhostsfile; do
	verbose $tid $i percent
	case "$i" in
	localcommand|userknownhostsfile)
		# Any test that's going to actually make a connection needs
		# to use the real username.
		REMUSER=$USER ;;
	*)
		REMUSER=remuser ;;
	esac
	if [ "$i" = "$localcommand" ]; then
		trial $i '%T' NONE
	fi
	# Matches implementation in readconf.c:ssh_connection_hash()
	if [ ! -z "${OPENSSL_BIN}" ]; then
		HASH=`printf "${HOSTNAME}127.0.0.1${PORT}${REMUSER}" |
		    $OPENSSL_BIN sha1 | cut -f2 -d' '`
		trial $i '%C' $HASH
	fi
	trial $i '%%' '%'
	trial $i '%i' $USERID
	trial $i '%h' 127.0.0.1
	trial $i '%L' $HOST
	trial $i '%l' $HOSTNAME
	trial $i '%n' somehost
	trial $i '%k' localhost-with-alias
	trial $i '%p' $PORT
	trial $i '%r' $REMUSER
	trial $i '%u' $USER
	# We can't specify a full path outside the regress dir, so skip tests
	# containing %d for UserKnownHostsFile, and %r can't refer to itself.
	if [ "$i" != "userknownhostsfile" ] && [ "$i" != "user" ] && \
	     [ "$i" != "user-l" ] && [ "$i" != "user-at" ]; then
		trial $i '%d' $HOME
		in='%%/%i/%h/%d/%L/%l/%n/%p/%r/%u'
		out="%/$USERID/127.0.0.1/$HOME/$HOST/$HOSTNAME/somehost/$PORT/$REMUSER/$USER"
		if [ ! -z "${HASH}" ]; then
			in="$in/%C"
			out="$out/$HASH"
		fi
		trial $i "$in" "$out"
	fi
done

# Subset of above since we don't expand shell-style variables on anything that
# runs a command because the shell will expand those.
for i in controlpath identityagent forwardagent localforward remoteforward \
    user user-l user-at setenv userknownhostsfile; do
	verbose $tid $i dollar
	FOO=bar
	export FOO
	trial $i '${FOO}' $FOO
done


# A subset of options support tilde expansion
for i in controlpath identityagent forwardagent; do
	verbose $tid $i tilde
	trial $i '~' $HOME/
	trial $i '~/.ssh' $HOME/.ssh
done
