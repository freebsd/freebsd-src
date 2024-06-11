#	$OpenBSD: agent-pkcs11-restrict.sh,v 1.1 2023/12/18 14:49:39 djm Exp $
#	Placed in the Public Domain.

tid="pkcs11 agent constraint test"

p11_setup || skip "No PKCS#11 library found"

rm -f $SSH_AUTH_SOCK $OBJ/agent.log $OBJ/host_[abcx]* $OBJ/user_[abcx]*
rm -f $OBJ/sshd_proxy_host* $OBJ/ssh_output* $OBJ/expect_*
rm -f $OBJ/ssh_proxy[._]* $OBJ/command $OBJ/authorized_keys_*

trace "generate host keys"
for h in a b x ca ; do
	$SSHKEYGEN -q -t ed25519 -C host_$h -N '' -f $OBJ/host_$h || \
		fatal "ssh-keygen hostkey failed"
done

# XXX test CA hostcerts too.

key_for() {
	case $h in
	a) K="${SSH_SOFTHSM_DIR}/RSA.pub" ;;
	b) K="${SSH_SOFTHSM_DIR}/EC.pub" ;;
	*) K="" ;;
	esac
	export K
}

SSH_AUTH_SOCK="$OBJ/agent.sock"
export SSH_AUTH_SOCK
rm -f $SSH_AUTH_SOCK
trace "start agent"
${SSHAGENT} ${EXTRA_AGENT_ARGS} -d -a $SSH_AUTH_SOCK > $OBJ/agent.log 2>&1 &
AGENT_PID=$!
trap "kill $AGENT_PID" EXIT
for x in 0 1 2 3 4 ; do
	# Give it a chance to start
	${SSHADD} -l > /dev/null 2>&1
	r=$?
	test $r -eq 1 && break
	sleep 1
done
if [ $r -ne 1 ]; then
	fatal "ssh-add -l did not fail with exit code 1 (got $r)"
fi

# XXX a lot of this is a copy of agent-restrict.sh, but I couldn't see a nice
# way to factor it out -djm

trace "prepare client config"
egrep -vi '(identityfile|hostname|hostkeyalias|proxycommand)' \
	$OBJ/ssh_proxy > $OBJ/ssh_proxy.bak
cat << _EOF > $OBJ/ssh_proxy
IdentitiesOnly yes
ForwardAgent yes
ExitOnForwardFailure yes
_EOF
cp $OBJ/ssh_proxy $OBJ/ssh_proxy_noid
for h in a b ; do
	key_for $h
	cat << _EOF >> $OBJ/ssh_proxy
Host host_$h
	Hostname host_$h
	HostkeyAlias host_$h
	IdentityFile $K
	ProxyCommand ${SUDO} env SSH_SK_HELPER=\"$SSH_SK_HELPER\" ${OBJ}/sshd-log-wrapper.sh -i -f $OBJ/sshd_proxy_host_$h
_EOF
	# Variant with no specified keys.
	cat << _EOF >> $OBJ/ssh_proxy_noid
Host host_$h
	Hostname host_$h
	HostkeyAlias host_$h
	ProxyCommand ${SUDO} env SSH_SK_HELPER=\"$SSH_SK_HELPER\" ${OBJ}/sshd-log-wrapper.sh -i -f $OBJ/sshd_proxy_host_$h
_EOF
done
cat $OBJ/ssh_proxy.bak >> $OBJ/ssh_proxy
cat $OBJ/ssh_proxy.bak >> $OBJ/ssh_proxy_noid

LC_ALL=C
export LC_ALL
echo "SetEnv LC_ALL=${LC_ALL}" >> sshd_proxy

trace "prepare known_hosts"
rm -f $OBJ/known_hosts
for h in a b x ; do
	(printf "host_$h " ; cat $OBJ/host_${h}.pub) >> $OBJ/known_hosts
done

trace "prepare server configs"
egrep -vi '(hostkey|pidfile)' $OBJ/sshd_proxy \
	> $OBJ/sshd_proxy.bak
for h in a b ; do
	cp $OBJ/sshd_proxy.bak $OBJ/sshd_proxy_host_$h
	cat << _EOF >> $OBJ/sshd_proxy_host_$h
ExposeAuthInfo yes
Hostkey $OBJ/host_$h
_EOF
	cp $OBJ/sshd_proxy_host_$h $OBJ/sshd_proxy_host_${h}.bak
done

trace "prepare authorized_keys"
cat >> $OBJ/command << EOF
#!/bin/sh
echo USERAUTH
cat \$SSH_USER_AUTH
echo AGENT
if $SSHADD -ql >/dev/null 2>&1 ; then
	$SSHADD -L | cut -d' ' -f1-2 | sort
else
	echo NONE
fi
EOF
chmod a+x $OBJ/command
>$OBJ/authorized_keys_$USER
for h in a b ; do
	key_for $h
	(printf "%s" "restrict,agent-forwarding,command=\"$OBJ/command\" ";
	 cat $K) >> $OBJ/authorized_keys_$USER
done

trace "unrestricted keys"
$SSHADD -qD >/dev/null || fatal "clear agent failed"
p11_ssh_add -qs ${TEST_SSH_PKCS11} ||
	fatal "failed to add keys"
for h in a b ; do
	key_for $h
	echo USERAUTH > $OBJ/expect_$h
	printf "publickey " >> $OBJ/expect_$h
	cat $K >> $OBJ/expect_$h
	echo AGENT >> $OBJ/expect_$h
	$SSHADD -L | cut -d' ' -f1-2 | sort >> $OBJ/expect_$h
	${SSH} -F $OBJ/ssh_proxy -oIdentityFile=$K \
	    host_$h true > $OBJ/ssh_output || fatal "test ssh $h failed"
	cmp $OBJ/expect_$h $OBJ/ssh_output || fatal "unexpected output"
done

trace "restricted to different host"
$SSHADD -qD >/dev/null || fatal "clear agent failed"
p11_ssh_add -q -h host_x -s ${TEST_SSH_PKCS11} -H $OBJ/known_hosts ||
	fatal "failed to add keys"
for h in a b ; do
	key_for $h
	${SSH} -F $OBJ/ssh_proxy -oIdentityFile=$K \
	    host_$h true > $OBJ/ssh_output && fatal "test ssh $h succeeded"
done

trace "restricted to destination host"
$SSHADD -qD >/dev/null || fatal "clear agent failed"
p11_ssh_add -q -h host_a -h host_b -s ${TEST_SSH_PKCS11} -H $OBJ/known_hosts ||
	fatal "failed to add keys"
for h in a b ; do
	key_for $h
	echo USERAUTH > $OBJ/expect_$h
	printf "publickey " >> $OBJ/expect_$h
	cat $K >> $OBJ/expect_$h
	echo AGENT >> $OBJ/expect_$h
	echo NONE >> $OBJ/expect_$h
	${SSH} -F $OBJ/ssh_proxy -oIdentityFile=$K \
	    host_$h true > $OBJ/ssh_output || fatal "test ssh $h failed"
	cmp $OBJ/expect_$h $OBJ/ssh_output || fatal "unexpected output"
done

trace "restricted multihop"
$SSHADD -qD >/dev/null || fatal "clear agent failed"
p11_ssh_add -q -h host_a -h "host_a>host_b" \
    -s ${TEST_SSH_PKCS11} -H $OBJ/known_hosts || fatal "failed to add keys"
key_for a
AK=$K
key_for b
BK=$K
# Prepare authorized_keys file to additionally ssh to host_b
_command="echo LOCAL ; ${OBJ}/command ; echo REMOTE; ${SSH} -AF $OBJ/ssh_proxy -oIdentityFile=$BK host_b"
(printf "%s" "restrict,agent-forwarding,command=\"$_command\" ";
 cat $BK) > $OBJ/authorized_keys_a
grep -vi AuthorizedKeysFile $OBJ/sshd_proxy_host_a.bak > $OBJ/sshd_proxy_host_a
echo "AuthorizedKeysFile $OBJ/authorized_keys_a" >> $OBJ/sshd_proxy_host_a
# Prepare expected output from both hosts.
echo LOCAL > $OBJ/expect_a
echo USERAUTH >> $OBJ/expect_a
printf "publickey " >> $OBJ/expect_a
cat $AK >> $OBJ/expect_a
echo AGENT >> $OBJ/expect_a
$SSHADD -L | cut -d' ' -f1-2 | sort >> $OBJ/expect_a
echo REMOTE >> $OBJ/expect_a
echo USERAUTH >> $OBJ/expect_a
printf "publickey " >> $OBJ/expect_a
cat $BK >> $OBJ/expect_a
echo AGENT >> $OBJ/expect_a
echo NONE >> $OBJ/expect_a
${SSH} -AF $OBJ/ssh_proxy -oIdentityFile=$AK \
    host_a whatever > $OBJ/ssh_output || fatal "test ssh $h failed"
cmp $OBJ/expect_a $OBJ/ssh_output || fatal "unexpected output"

