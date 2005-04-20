#	$OpenBSD: try-ciphers.sh,v 1.9 2004/02/28 13:44:45 dtucker Exp $
#	Placed in the Public Domain.

tid="try ciphers"

ciphers="aes128-cbc 3des-cbc blowfish-cbc cast128-cbc arcfour 
	aes192-cbc aes256-cbc rijndael-cbc@lysator.liu.se
	aes128-ctr aes192-ctr aes256-ctr"
macs="hmac-sha1 hmac-md5 hmac-sha1-96 hmac-md5-96"

for c in $ciphers; do
	for m in $macs; do
		trace "proto 2 cipher $c mac $m"
		verbose "test $tid: proto 2 cipher $c mac $m"
		${SSH} -F $OBJ/ssh_proxy -2 -m $m -c $c somehost true
		if [ $? -ne 0 ]; then
			fail "ssh -2 failed with mac $m cipher $c"
		fi
	done
done

ciphers="3des blowfish"
for c in $ciphers; do
	trace "proto 1 cipher $c"
	verbose "test $tid: proto 1 cipher $c"
	${SSH} -F $OBJ/ssh_proxy -1 -c $c somehost true
	if [ $? -ne 0 ]; then
		fail "ssh -1 failed with cipher $c"
	fi
done

if ! ${SSH} -oCiphers=acss@openssh.org 2>&1 | grep "Bad SSH2 cipher" >/dev/null
then

echo "Ciphers acss@openssh.org" >> $OBJ/sshd_proxy
c=acss@openssh.org
for m in $macs; do
	trace "proto 2 $c mac $m"
	verbose "test $tid: proto 2 cipher $c mac $m"
	${SSH} -F $OBJ/ssh_proxy -2 -m $m -c $c somehost true
	if [ $? -ne 0 ]; then
		fail "ssh -2 failed with mac $m cipher $c"
	fi
done

fi
