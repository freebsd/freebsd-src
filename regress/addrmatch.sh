#	$OpenBSD: addrmatch.sh,v 1.3 2010/02/09 04:57:36 djm Exp $
#	Placed in the Public Domain.

tid="address match"

mv $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

run_trial()
{
	user="$1"; addr="$2"; host="$3"; expected="$4"; descr="$5"

	verbose "test $descr for $user $addr $host"
	result=`${SSHD} -f $OBJ/sshd_proxy -T \
	    -C user=${user},addr=${addr},host=${host} | \
	    awk '/^passwordauthentication/ {print $2}'`
	if [ "$result" != "$expected" ]; then
		fail "failed for $user $addr $host: expected $expected, got $result"
	fi
}

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >>$OBJ/sshd_proxy <<EOD
PasswordAuthentication no
Match Address 192.168.0.0/16,!192.168.30.0/24,10.0.0.0/8,host.example.com
	PasswordAuthentication yes
Match Address 1.1.1.1,::1,!::3,2000::/16
	PasswordAuthentication yes
EOD

run_trial user 192.168.0.1 somehost yes		"permit, first entry"
run_trial user 192.168.30.1 somehost no		"deny, negative match"
run_trial user 19.0.0.1 somehost no		"deny, no match"
run_trial user 10.255.255.254 somehost yes	"permit, list middle"
run_trial user 192.168.30.1 192.168.0.1 no	"deny, faked IP in hostname"
run_trial user 1.1.1.1 somehost.example.com yes	"permit, bare IP4 address"
test "$TEST_SSH_IPV6" = "no" && exit
run_trial user ::1 somehost.example.com	 yes	"permit, bare IP6 address"
run_trial user ::2 somehost.exaple.com no	"deny IPv6"
run_trial user ::3 somehost no			"deny IP6 negated"
run_trial user ::4 somehost no			"deny, IP6 no match"
run_trial user 2000::1 somehost yes		"permit, IP6 network"
run_trial user 2001::1 somehost no		"deny, IP6 network"

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
rm $OBJ/sshd_proxy_bak
