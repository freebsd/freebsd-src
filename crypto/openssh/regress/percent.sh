#	$OpenBSD: percent.sh,v 1.14 2022/02/20 03:47:26 dtucker Exp $
#	Placed in the Public Domain.

tid="percent expansions"

if [ -x "/usr/xpg4/bin/id" ]; then
	PATH=/usr/xpg4/bin:$PATH
	export PATH
fi

USER=`id -u -n`
USERID=`id -u`
HOST=`hostname | cut -f1 -d.`
HOSTNAME=`hostname`

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
	*)
		got=`${SSH} -F $OBJ/ssh_proxy -o $opt="$arg" -G \
		    remuser@somehost | awk '$1=="'$opt'"{print $2}'`
	esac
	if [ "$got" != "$expect" ]; then
		fail "$opt=$arg expect $expect got $got"
	fi
}

for i in matchexec localcommand remotecommand controlpath identityagent \
    forwardagent localforward remoteforward userknownhostsfile; do
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
	HASH=`printf "${HOSTNAME}127.0.0.1${PORT}$REMUSER" |
	    $OPENSSL_BIN sha1 | cut -f2 -d' '`
	trial $i '%%' '%'
	trial $i '%C' $HASH
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
	# containing %d for UserKnownHostsFile
	if [ "$i" != "userknownhostsfile" ]; then
		trial $i '%d' $HOME
		trial $i '%%/%C/%i/%h/%d/%L/%l/%n/%p/%r/%u' \
		    "%/$HASH/$USERID/127.0.0.1/$HOME/$HOST/$HOSTNAME/somehost/$PORT/$REMUSER/$USER"
	fi
done

# Subset of above since we don't expand shell-style variables on anything that
# runs a command because the shell will expand those.
for i in controlpath identityagent forwardagent localforward remoteforward \
    userknownhostsfile; do
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
