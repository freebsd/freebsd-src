#	$OpenBSD: connect-privsep.sh,v 1.1 2002/03/21 21:45:07 markus Exp $
#	Placed in the Public Domain.

tid="proxy connect with privsep"

echo 'UsePrivilegeSeparation yes' >> $OBJ/sshd_proxy

for p in 1 2; do
	${SSH} -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		fail "ssh privsep+proxyconnect protocol $p failed"
	fi
done
