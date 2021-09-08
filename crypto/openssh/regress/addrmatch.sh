#	$OpenBSD: addrmatch.sh,v 1.6 2020/08/28 03:17:13 dtucker Exp $
#	Placed in the Public Domain.

tid="address match"

mv $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

run_trial()
{
	user="$1"; addr="$2"; host="$3"; laddr="$4"; lport="$5"
	expected="$6"; descr="$7"

	verbose "test $descr for $user $addr $host"
	result=`${SSHD} -f $OBJ/sshd_proxy -T \
	    -C user=${user},addr=${addr},host=${host},laddr=${laddr},lport=${lport} | \
	    awk '/^forcecommand/ {print $2}'`
	if [ "$result" != "$expected" ]; then
		fail "failed '$descr' expected $expected got $result"
	fi
}

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >>$OBJ/sshd_proxy <<EOD
ForceCommand nomatch
Match Address 192.168.0.0/16,!192.168.30.0/24,10.0.0.0/8,host.example.com
	ForceCommand match1
Match Address 1.1.1.1,::1,!::3,2000::/16
	ForceCommand match2
Match LocalAddress 127.0.0.1,::1
	ForceCommand match3
Match LocalPort 5678
	ForceCommand match4
EOD

run_trial user 192.168.0.1 somehost 1.2.3.4 1234 match1 "first entry"
run_trial user 192.168.30.1 somehost 1.2.3.4 1234 nomatch "negative match"
run_trial user 19.0.0.1 somehost 1.2.3.4 1234 nomatch "no match"
run_trial user 10.255.255.254 somehost 1.2.3.4 1234 match1 "list middle"
run_trial user 192.168.30.1 192.168.0.1 1.2.3.4 1234 nomatch "faked IP in hostname"
run_trial user 1.1.1.1 somehost.example.com 1.2.3.4 1234 match2 "bare IP4 address"
run_trial user 19.0.0.1 somehost 127.0.0.1 1234 match3 "localaddress"
run_trial user 19.0.0.1 somehost 1.2.3.4 5678 match4 "localport"

if test "$TEST_SSH_IPV6" != "no"; then
run_trial user ::1 somehost.example.com ::2 1234 match2 "bare IP6 address"
run_trial user ::2 somehost.example.com ::2 1234 nomatch "deny IPv6"
run_trial user ::3 somehost ::2 1234 nomatch "IP6 negated"
run_trial user ::4 somehost ::2 1234 nomatch "IP6 no match"
run_trial user 2000::1 somehost ::2 1234 match2 "IP6 network"
run_trial user 2001::1 somehost ::2 1234 nomatch "IP6 network"
run_trial user ::5 somehost ::1 1234 match3 "IP6 localaddress"
run_trial user ::5 somehost ::2 5678 match4 "IP6 localport"
fi

#
# Check that we catch invalid address/mask in Match Address/Localaddress
#
for i in 10.0.1.0/8 10.0.0.1/24 2000:aa:bb:01::/56; do
    for a in address localaddress; do
	verbose "test invalid Match $a $i"
	echo "Match $a $i" > $OBJ/sshd_proxy
	${SUDO} ${SSHD} -f $OBJ/sshd_proxy -t >/dev/null 2>&1 && \
	    fail "accepted invalid match $a $i"
    done
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
rm $OBJ/sshd_proxy_bak
