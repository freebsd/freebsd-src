#	$OpenBSD: agent-restrict.sh,v 1.5 2022/01/13 04:53:16 dtucker Exp $
#	Placed in the Public Domain.

tid="agent restrictions"

SSH_AUTH_SOCK="$OBJ/agent.sock"
export SSH_AUTH_SOCK
rm -f $SSH_AUTH_SOCK $OBJ/agent.log $OBJ/host_[abcdex]* $OBJ/user_[abcdex]*
rm -f $OBJ/sshd_proxy_host* $OBJ/ssh_output* $OBJ/expect_*
rm -f $OBJ/ssh_proxy[._]* $OBJ/command

verbose "generate keys"
for h in a b c d e x ca ; do
	$SSHKEYGEN -q -t ed25519 -C host_$h -N '' -f $OBJ/host_$h || \
		fatal "ssh-keygen hostkey failed"
	$SSHKEYGEN -q -t ed25519 -C user_$h -N '' -f $OBJ/user_$h || \
		fatal "ssh-keygen userkey failed"
done

# Make some hostcerts
for h in d e ; do
	id="host_$h"
	$SSHKEYGEN -q -s $OBJ/host_ca -I $id -n $id -h $OBJ/host_${h}.pub || \
		fatal "ssh-keygen certify failed"
done

verbose "prepare client config"
egrep -vi '(identityfile|hostname|hostkeyalias|proxycommand)' \
	$OBJ/ssh_proxy > $OBJ/ssh_proxy.bak
cat << _EOF > $OBJ/ssh_proxy
IdentitiesOnly yes
ForwardAgent yes
ExitOnForwardFailure yes
_EOF
cp $OBJ/ssh_proxy $OBJ/ssh_proxy_noid
for h in a b c d e ; do
	cat << _EOF >> $OBJ/ssh_proxy
Host host_$h
	Hostname host_$h
	HostkeyAlias host_$h
	IdentityFile $OBJ/user_$h
	ProxyCommand ${SUDO} env SSH_SK_HELPER=\"$SSH_SK_HELPER\" sh ${SRC}/sshd-log-wrapper.sh ${TEST_SSHD_LOGFILE} ${SSHD} -i -f $OBJ/sshd_proxy_host_$h
_EOF
	# Variant with no specified keys.
	cat << _EOF >> $OBJ/ssh_proxy_noid
Host host_$h
	Hostname host_$h
	HostkeyAlias host_$h
	ProxyCommand ${SUDO} env SSH_SK_HELPER=\"$SSH_SK_HELPER\" sh ${SRC}/sshd-log-wrapper.sh ${TEST_SSHD_LOGFILE} ${SSHD} -i -f $OBJ/sshd_proxy_host_$h
_EOF
done
cat $OBJ/ssh_proxy.bak >> $OBJ/ssh_proxy
cat $OBJ/ssh_proxy.bak >> $OBJ/ssh_proxy_noid

LC_ALL=C
export LC_ALL
echo "SetEnv LC_ALL=${LC_ALL}" >> sshd_proxy

verbose "prepare known_hosts"
rm -f $OBJ/known_hosts
for h in a b c x ; do
	(printf "host_$h " ; cat $OBJ/host_${h}.pub) >> $OBJ/known_hosts
done
(printf "@cert-authority host_* " ; cat $OBJ/host_ca.pub) >> $OBJ/known_hosts

verbose "prepare server configs"
egrep -vi '(hostkey|pidfile)' $OBJ/sshd_proxy \
	> $OBJ/sshd_proxy.bak
for h in a b c d e; do
	cp $OBJ/sshd_proxy.bak $OBJ/sshd_proxy_host_$h
	cat << _EOF >> $OBJ/sshd_proxy_host_$h
ExposeAuthInfo yes
PidFile none
Hostkey $OBJ/host_$h
_EOF
done
for h in d e ; do
	echo "HostCertificate $OBJ/host_${h}-cert.pub" \
		>> $OBJ/sshd_proxy_host_$h
done
# Create authorized_keys with canned command.
reset_keys() {
	_whichcmd="$1"
	_command=""
	case "$_whichcmd" in
	authinfo)	_command="cat \$SSH_USER_AUTH" ;;
	keylist)		_command="$SSHADD -L | cut -d' ' -f-2 | sort" ;;
	*)		fatal "unsupported command $_whichcmd" ;;
	esac
	trace "reset keys"
	>$OBJ/authorized_keys_$USER
	for h in e d c b a; do
		(printf "%s" "restrict,agent-forwarding,command=\"$_command\" ";
		 cat $OBJ/user_$h.pub) >> $OBJ/authorized_keys_$USER
	done
}
# Prepare a key for comparison with ExposeAuthInfo/$SSH_USER_AUTH.
expect_key() {
	_key="$OBJ/${1}.pub"
	_file="$OBJ/$2"
	(printf "publickey " ; cut -d' ' -f-2 $_key) > $_file
}
# Prepare expect_* files to compare against authinfo forced command to ensure
# keys used for authentication match.
reset_expect_keys() {
	for u in a b c d e; do
		expect_key user_$u expect_$u
	done
}
# ssh to host, expecting success and that output matched expectation for
# that host (expect_$h file).
expect_succeed() {
	_id="$1"
	_case="$2"
	shift; shift; _extra="$@"
	_host="host_$_id"
	trace "connect $_host expect success"
	rm -f $OBJ/ssh_output
	${SSH} $_extra -F $OBJ/ssh_proxy $_host true > $OBJ/ssh_output
	_s=$?
	test $_s -eq 0 || fail "host $_host $_case fail, exit status $_s"
	diff $OBJ/ssh_output $OBJ/expect_${_id} ||
		fail "unexpected ssh output"
}
# ssh to host using explicit key, expecting success and that the key was
# actually used for authentication.
expect_succeed_key() {
	_id="$1"
	_key="$2"
	_case="$3"
	shift; shift; shift; _extra="$@"
	_host="host_$_id"
	trace "connect $_host expect success, with key $_key"
	_keyfile="$OBJ/$_key"
	rm -f $OBJ/ssh_output
	${SSH} $_extra -F $OBJ/ssh_proxy_noid \
	    -oIdentityFile=$_keyfile $_host true > $OBJ/ssh_output
	_s=$?
	test $_s -eq 0 || fail "host $_host $_key $_case fail, exit status $_s"
	expect_key $_key expect_key
	diff $OBJ/ssh_output $OBJ/expect_key ||
		fail "incorrect key used for authentication"
}
# ssh to a host, expecting it to fail.
expect_fail() {
	_host="$1"
	_case="$2"
	shift; shift; _extra="$@"
	trace "connect $_host expect failure"
	${SSH} $_extra -F $OBJ/ssh_proxy $_host true >/dev/null && \
		fail "host $_host $_case succeeded unexpectedly"
}
# ssh to a host using an explicit key, expecting it to fail.
expect_fail_key() {
	_id="$1"
	_key="$2"
	_case="$3"
	shift; shift; shift; _extra="$@"
	_host="host_$_id"
	trace "connect $_host expect failure, with key $_key"
	_keyfile="$OBJ/$_key"
	${SSH} $_extra -F $OBJ/ssh_proxy_noid -oIdentityFile=$_keyfile \
	    $_host true > $OBJ/ssh_output && \
		fail "host $_host $_key $_case succeeded unexpectedly"
}
# Move the private key files out of the way to force use of agent-hosted keys.
hide_privatekeys() {
	trace "hide private keys"
	for u in a b c d e x; do
		mv $OBJ/user_$u $OBJ/user_x$u || fatal "hide privkey $u"
	done
}
# Put the private key files back.
restore_privatekeys() {
	trace "restore private keys"
	for u in a b c d e x; do
		mv $OBJ/user_x$u $OBJ/user_$u || fatal "restore privkey $u"
	done
}
clear_agent() {
	${SSHADD} -D > /dev/null 2>&1 || fatal "clear agent failed"
}

reset_keys authinfo
reset_expect_keys

verbose "authentication w/o agent"
for h in a b c d e ; do
	expect_succeed $h "w/o agent"
	wrongkey=user_e
	test "$h" = "e" && wrongkey=user_a
	expect_succeed_key $h $wrongkey "\"wrong\" key w/o agent"
done
hide_privatekeys
for h in a b c d e ; do
	expect_fail $h "w/o agent"
done
restore_privatekeys

verbose "start agent"
${SSHAGENT} ${EXTRA_AGENT_ARGS} -d -a $SSH_AUTH_SOCK > $OBJ/agent.log 2>&1 &
AGENT_PID=$!
trap "kill $AGENT_PID" EXIT
sleep 4 # Give it a chance to start
# Check that it's running.
${SSHADD} -l > /dev/null 2>&1
if [ $? -ne 1 ]; then
	fail "ssh-add -l did not fail with exit code 1"
fi

verbose "authentication with agent (no restrict)"
for u in a b c d e x; do
	$SSHADD -q $OBJ/user_$u || fatal "add key $u unrestricted"
done
hide_privatekeys
for h in a b c d e ; do
	expect_succeed $h "with agent"
	wrongkey=user_e
	test "$h" = "e" && wrongkey=user_a
	expect_succeed_key $h $wrongkey "\"wrong\" key with agent"
done

verbose "unrestricted keylist"
reset_keys keylist
rm -f $OBJ/expect_list.pre
# List of keys from agent should contain everything.
for u in a b c d e x; do
	cut -d " " -f-2 $OBJ/user_${u}.pub >> $OBJ/expect_list.pre
done
sort $OBJ/expect_list.pre > $OBJ/expect_list
for h in a b c d e; do
	cp $OBJ/expect_list $OBJ/expect_$h
	expect_succeed $h "unrestricted keylist"
done
restore_privatekeys

verbose "authentication with agent (basic restrict)"
reset_keys authinfo
reset_expect_keys
for h in a b c d e; do
	$SSHADD -h host_$h -H $OBJ/known_hosts -q $OBJ/user_$h \
		|| fatal "add key $u basic restrict"
done
# One more, unrestricted
$SSHADD -q $OBJ/user_x || fatal "add unrestricted key"
hide_privatekeys
# Authentication to host with expected key should work.
for h in a b c d e ; do
	expect_succeed $h "with agent"
done
# Authentication to host with incorrect key should fail.
verbose "authentication with agent incorrect key (basic restrict)"
for h in a b c d e ; do
	wrongkey=user_e
	test "$h" = "e" && wrongkey=user_a
	expect_fail_key $h $wrongkey "wrong key with agent (basic restrict)"
done

verbose "keylist (basic restrict)"
reset_keys keylist
# List from forwarded agent should contain only user_x - the unrestricted key.
cut -d " " -f-2 $OBJ/user_x.pub > $OBJ/expect_list
for h in a b c d e; do
	cp $OBJ/expect_list $OBJ/expect_$h
	expect_succeed $h "keylist (basic restrict)"
done
restore_privatekeys

verbose "username"
reset_keys authinfo
reset_expect_keys
for h in a b c d e; do
	$SSHADD -h "${USER}@host_$h" -H $OBJ/known_hosts -q $OBJ/user_$h \
		|| fatal "add key $u basic restrict"
done
hide_privatekeys
for h in a b c d e ; do
	expect_succeed $h "wildcard user"
done
restore_privatekeys

verbose "username wildcard"
reset_keys authinfo
reset_expect_keys
for h in a b c d e; do
	$SSHADD -h "*@host_$h" -H $OBJ/known_hosts -q $OBJ/user_$h \
		|| fatal "add key $u basic restrict"
done
hide_privatekeys
for h in a b c d e ; do
	expect_succeed $h "wildcard user"
done
restore_privatekeys

verbose "username incorrect"
reset_keys authinfo
reset_expect_keys
for h in a b c d e; do
	$SSHADD -h "--BADUSER@host_$h" -H $OBJ/known_hosts -q $OBJ/user_$h \
		|| fatal "add key $u basic restrict"
done
hide_privatekeys
for h in a b c d e ; do
	expect_fail $h "incorrect user"
done
restore_privatekeys


verbose "agent restriction honours certificate principal"
reset_keys authinfo
reset_expect_keys
clear_agent
$SSHADD -h host_e -H $OBJ/known_hosts -q $OBJ/user_d || fatal "add key"
hide_privatekeys
expect_fail d "restricted agent w/ incorrect cert principal"
restore_privatekeys

# Prepares the script used to drive chained ssh connections for the
# multihop tests. Believe me, this is easier than getting the escaping
# right for 5 hops on the command-line...
prepare_multihop_script() {
	MULTIHOP_RUN=$OBJ/command
	cat << _EOF > $MULTIHOP_RUN
#!/bin/sh
#set -x
me="\$1" ; shift
next="\$1"
if test ! -z "\$me" ; then 
	rm -f $OBJ/done
	echo "HOSTNAME host_\$me"
	echo "AUTHINFO"
	cat \$SSH_USER_AUTH
fi
echo AGENT
$SSHADD -L | egrep "^ssh" | cut -d" " -f-2 | sort
if test -z "\$next" ; then 
	touch $OBJ/done
	echo "FINISH"
	e=0
else
	echo NEXT
	${SSH} -F $OBJ/ssh_proxy_noid -oIdentityFile=$OBJ/user_a \
		host_\$next $MULTIHOP_RUN "\$@"
	e=\$?
fi
echo "COMPLETE \"\$me\""
if test ! -z "\$me" ; then 
	if test ! -f $OBJ/done ; then
		echo "DONE MARKER MISSING"
		test \$e -eq 0 && e=63
	fi
fi
exit \$e
_EOF
	chmod u+x $MULTIHOP_RUN
}

# Prepare expected output for multihop tests at expect_a
prepare_multihop_expected() {
	_keys="$1"
	_hops="a b c d e"
	test -z "$2" || _hops="$2"
	_revhops=$(echo "$_hops" | rev)
	_lasthop=$(echo "$_hops" | sed 's/.* //')

	rm -f $OBJ/expect_keys
	for h in a b c d e; do
		cut -d" " -f-2 $OBJ/user_${h}.pub >> $OBJ/expect_keys
	done
	rm -f $OBJ/expect_a
	echo "AGENT" >> $OBJ/expect_a
	test "x$_keys" = "xnone" || sort $OBJ/expect_keys >> $OBJ/expect_a
	echo "NEXT" >> $OBJ/expect_a
	for h in $_hops ; do 
		echo "HOSTNAME host_$h" >> $OBJ/expect_a
		echo "AUTHINFO" >> $OBJ/expect_a
		(printf "publickey " ; cut -d" " -f-2 $OBJ/user_a.pub) >> $OBJ/expect_a
		echo "AGENT" >> $OBJ/expect_a
		if test "x$_keys" = "xall" ; then
			sort $OBJ/expect_keys >> $OBJ/expect_a
		fi
		if test "x$h" != "x$_lasthop" ; then
			if test "x$_keys" = "xfiltered" ; then
				cut -d" " -f-2 $OBJ/user_a.pub >> $OBJ/expect_a
			fi
			echo "NEXT" >> $OBJ/expect_a
		fi
	done
	echo "FINISH" >> $OBJ/expect_a
	for h in $_revhops "" ; do 
		echo "COMPLETE \"$h\"" >> $OBJ/expect_a
	done
}

prepare_multihop_script
cp $OBJ/user_a.pub $OBJ/authorized_keys_$USER # only one key used.

verbose "multihop without agent"
clear_agent
prepare_multihop_expected none
$MULTIHOP_RUN "" a b c d e > $OBJ/ssh_output || fail "multihop no agent ssh failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"

verbose "multihop agent unrestricted"
clear_agent
$SSHADD -q $OBJ/user_[abcde]
prepare_multihop_expected all
$MULTIHOP_RUN "" a b c d e > $OBJ/ssh_output || fail "multihop no agent ssh failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"

verbose "multihop restricted"
clear_agent
prepare_multihop_expected filtered
# Add user_a, with permission to connect through the whole chain.
$SSHADD -h host_a -h "host_a>host_b" -h "host_b>host_c" \
	-h "host_c>host_d" -h "host_d>host_e" \
	-H $OBJ/known_hosts -q $OBJ/user_a \
	|| fatal "add key user_a multihop"
# Add the other keys, bound to a unused host.
$SSHADD -q -h host_x -H $OBJ/known_hosts $OBJ/user_[bcde] || fail "add keys"
hide_privatekeys
$MULTIHOP_RUN "" a b c d e > $OBJ/ssh_output || fail "multihop ssh failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"
restore_privatekeys

verbose "multihop username"
$SSHADD -h host_a -h "host_a>${USER}@host_b" -h "host_b>${USER}@host_c" \
	-h "host_c>${USER}@host_d"  -h "host_d>${USER}@host_e" \
	-H $OBJ/known_hosts -q $OBJ/user_a || fatal "add key user_a multihop"
hide_privatekeys
$MULTIHOP_RUN "" a b c d e > $OBJ/ssh_output || fail "multihop w/ user ssh failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"
restore_privatekeys

verbose "multihop wildcard username"
$SSHADD -h host_a -h "host_a>*@host_b" -h "host_b>*@host_c" \
	-h "host_c>*@host_d"  -h "host_d>*@host_e" \
	-H $OBJ/known_hosts -q $OBJ/user_a || fatal "add key user_a multihop"
hide_privatekeys
$MULTIHOP_RUN "" a b c d e > $OBJ/ssh_output || fail "multihop w/ user ssh failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"
restore_privatekeys

verbose "multihop wrong username"
$SSHADD -h host_a -h "host_a>*@host_b" -h "host_b>*@host_c" \
	-h "host_c>--BADUSER@host_d"  -h "host_d>*@host_e" \
	-H $OBJ/known_hosts -q $OBJ/user_a || fatal "add key user_a multihop"
hide_privatekeys
$MULTIHOP_RUN "" a b c d e > $OBJ/ssh_output && \
	fail "multihop with wrong user succeeded unexpectedly"
restore_privatekeys

verbose "multihop cycle no agent"
clear_agent
prepare_multihop_expected none "a b a a c d e"
$MULTIHOP_RUN "" a b a a c d e > $OBJ/ssh_output || \
	fail "multihop cycle no-agent fail"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"

verbose "multihop cycle agent unrestricted"
clear_agent
$SSHADD -q $OBJ/user_[abcde] || fail "add keys"
prepare_multihop_expected all "a b a a c d e"
$MULTIHOP_RUN "" a b a a c d e > $OBJ/ssh_output || \
	fail "multihop cycle agent ssh failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"

verbose "multihop cycle restricted deny"
clear_agent
$SSHADD -q -h host_x -H $OBJ/known_hosts $OBJ/user_[bcde] || fail "add keys"
$SSHADD -h host_a -h "host_a>host_b" -h "host_b>host_c" \
	-h "host_c>host_d" -h "host_d>host_e" \
	-H $OBJ/known_hosts -q $OBJ/user_a \
	|| fatal "add key user_a multihop"
prepare_multihop_expected filtered "a b a a c d e"
hide_privatekeys
$MULTIHOP_RUN "" a b a a c d e > $OBJ/ssh_output && \
	fail "multihop cycle restricted deny succeded unexpectedly"
restore_privatekeys

verbose "multihop cycle restricted allow"
clear_agent
$SSHADD -q -h host_x -H $OBJ/known_hosts $OBJ/user_[bcde] || fail "add keys"
$SSHADD -h host_a -h "host_a>host_b" -h "host_b>host_c" \
	-h "host_c>host_d" -h "host_d>host_e" \
	-h "host_b>host_a" -h "host_a>host_a" -h "host_a>host_c" \
	-H $OBJ/known_hosts -q $OBJ/user_a \
	|| fatal "add key user_a multihop"
prepare_multihop_expected filtered "a b a a c d e"
hide_privatekeys
$MULTIHOP_RUN "" a b a a c d e > $OBJ/ssh_output || \
	fail "multihop cycle restricted allow failed"
diff $OBJ/ssh_output $OBJ/expect_a || fail "unexpected ssh output"
restore_privatekeys

