#	$OpenBSD: key-options.sh,v 1.4 2017/04/30 23:34:55 djm Exp $
#	Placed in the Public Domain.

tid="key options"

origkeys="$OBJ/authkeys_orig"
authkeys="$OBJ/authorized_keys_${USER}"
cp $authkeys $origkeys

# Test command= forced command
for c in 'command="echo bar"' 'no-pty,command="echo bar"'; do
	sed "s/.*/$c &/" $origkeys >$authkeys
	verbose "key option $c"
	r=`${SSH} -q -F $OBJ/ssh_proxy somehost echo foo`
	if [ "$r" = "foo" ]; then
		fail "key option forced command not restricted"
	fi
	if [ "$r" != "bar" ]; then
		fail "key option forced command not executed"
	fi
done

# Test no-pty
sed 's/.*/no-pty &/' $origkeys >$authkeys
verbose "key option proto no-pty"
r=`${SSH} -q -F $OBJ/ssh_proxy somehost tty`
if [ -f "$r" ]; then
	fail "key option failed no-pty (pty $r)"
fi

# Test environment=
echo 'PermitUserEnvironment yes' >> $OBJ/sshd_proxy
sed 's/.*/environment="FOO=bar" &/' $origkeys >$authkeys
verbose "key option environment"
r=`${SSH} -q -F $OBJ/ssh_proxy somehost 'echo $FOO'`
if [ "$r" != "bar" ]; then
	fail "key option environment not set"
fi

# Test from= restriction
start_sshd
for f in 127.0.0.1 '127.0.0.0\/8'; do
	cat  $origkeys >$authkeys
	${SSH} -q -F $OBJ/ssh_proxy somehost true
	if [ $? -ne 0 ]; then
		fail "key option failed without restriction"
	fi

	sed 's/.*/from="'"$f"'" &/' $origkeys >$authkeys
	from=`head -1 $authkeys | cut -f1 -d ' '`
	verbose "key option $from"
	r=`${SSH} -q -F $OBJ/ssh_proxy somehost 'echo true'`
	if [ "$r" = "true" ]; then
		fail "key option $from not restricted"
	fi

	r=`${SSH} -q -F $OBJ/ssh_config somehost 'echo true'`
	if [ "$r" != "true" ]; then
		fail "key option $from not allowed but should be"
	fi
done

rm -f "$origkeys"
