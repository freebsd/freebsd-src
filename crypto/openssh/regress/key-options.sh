#	$OpenBSD: key-options.sh,v 1.2 2008/06/30 08:07:34 djm Exp $
#	Placed in the Public Domain.

tid="key options"

origkeys="$OBJ/authkeys_orig"
authkeys="$OBJ/authorized_keys_${USER}"
cp $authkeys $origkeys

# Test command= forced command
for p in 1 2; do
    for c in 'command="echo bar"' 'no-pty,command="echo bar"'; do
	sed "s/.*/$c &/" $origkeys >$authkeys
	verbose "key option proto $p $c"
	r=`${SSH} -$p -q -F $OBJ/ssh_proxy somehost echo foo`
	if [ "$r" = "foo" ]; then
		fail "key option forced command not restricted"
	fi
	if [ "$r" != "bar" ]; then
		fail "key option forced command not executed"
	fi
    done
done

# Test no-pty
sed 's/.*/no-pty &/' $origkeys >$authkeys
for p in 1 2; do
	verbose "key option proto $p no-pty"
	r=`${SSH} -$p -q -F $OBJ/ssh_proxy somehost tty`
	if [ -f "$r" ]; then
		fail "key option failed proto $p no-pty (pty $r)"
	fi
done

# Test environment=
echo 'PermitUserEnvironment yes' >> $OBJ/sshd_proxy
sed 's/.*/environment="FOO=bar" &/' $origkeys >$authkeys
for p in 1 2; do
	verbose "key option proto $p environment"
	r=`${SSH} -$p -q -F $OBJ/ssh_proxy somehost 'echo $FOO'`
	if [ "$r" != "bar" ]; then
		fail "key option environment not set"
	fi
done

# Test from= restriction
start_sshd
for p in 1 2; do
    for f in 127.0.0.1 '127.0.0.0\/8'; do
	cat  $origkeys >$authkeys
	${SSH} -$p -q -F $OBJ/ssh_proxy somehost true
	if [ $? -ne 0 ]; then
		fail "key option proto $p failed without restriction"
	fi

	sed 's/.*/from="'"$f"'" &/' $origkeys >$authkeys
	from=`head -1 $authkeys | cut -f1 -d ' '`
	verbose "key option proto $p $from"
	r=`${SSH} -$p -q -F $OBJ/ssh_proxy somehost 'echo true'`
	if [ "$r" = "true" ]; then
		fail "key option proto $p $from not restricted"
	fi

	r=`${SSH} -$p -q -F $OBJ/ssh_config somehost 'echo true'`
	if [ "$r" != "true" ]; then
		fail "key option proto $p $from not allowed but should be"
	fi
    done
done

rm -f "$origkeys"
