#	$OpenBSD: key-options.sh,v 1.10 2024/03/25 02:07:08 dtucker Exp $
#	Placed in the Public Domain.

tid="key options"

origkeys="$OBJ/authkeys_orig"
authkeys="$OBJ/authorized_keys_${USER}"
cp $authkeys $origkeys

# Allocating ptys can require privileges on some platforms.
skip_pty=""
if ! config_defined HAVE_OPENPTY && [ "x$SUDO" = "x" ]; then
	skip_pty="no openpty(3) and SUDO not set"
fi

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
expect_pty_succeed() {
	which=$1
	opts=$2
	rm -f $OBJ/data
	sed "s/.*/$opts &/" $origkeys >$authkeys
	verbose "key option pty $which"
	[ "x$skip_pty" != "x" ] && verbose "skipped because $skip_pty" && return
	${SSH} -ttq -F $OBJ/ssh_proxy somehost "tty > $OBJ/data; exit 0"
	if [ $? -ne 0 ] ; then
		fail "key option failed $which"
	else
		r=`cat $OBJ/data`
		case "$r" in
		/dev/*) ;;
		*)	fail "key option failed $which (pty $r)" ;;
		esac
	fi
}
expect_pty_fail() {
	which=$1
	opts=$2
	rm -f $OBJ/data
	sed "s/.*/$opts &/" $origkeys >$authkeys
	verbose "key option pty $which"
	[ "x$skip_pty" != "x" ] && verbose "skipped because $skip_pty" && return
	${SSH} -ttq -F $OBJ/ssh_proxy somehost "tty > $OBJ/data; exit 0"
	if [ $? -eq 0 ]; then
		r=`cat $OBJ/data`
		if [ -e "$r" ]; then
			fail "key option failed $which (pty $r)"
		fi
		case "$r" in
		/dev/*)	fail "key option failed $which (pty $r)" ;;
		*)	;;
		esac
	fi
}
# First ensure that we can allocate a pty by default.
expect_pty_succeed "default" ""
expect_pty_fail "no-pty" "no-pty"
expect_pty_fail "restrict" "restrict"
expect_pty_succeed "restrict,pty" "restrict,pty"

# Test environment=
# XXX this can fail if ~/.ssh/environment exists for the user running the test
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

check_valid_before() {
	which=$1
	opts=$2
	expect=$3
	sed "s/.*/$opts &/" $origkeys >$authkeys
	verbose "key option expiry-time $which"
	${SSH} -q -F $OBJ/ssh_proxy somehost true
	r=$?
	case "$expect" in
	fail)	test $r -eq 0 && fail "key option succeeded $which" ;;
	pass)	test $r -ne 0 && fail "key option failed $which" ;;
	*)	fatal "unknown expectation $expect" ;;
	esac
}
check_valid_before "default"	""				"pass"
check_valid_before "invalid"	'expiry-time="INVALID"'		"fail"
check_valid_before "expired"	'expiry-time="19990101"'	"fail"
check_valid_before "valid"	'expiry-time="20380101"'	"pass"

