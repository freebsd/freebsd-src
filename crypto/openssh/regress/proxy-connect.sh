#	$OpenBSD: proxy-connect.sh,v 1.5 2002/12/09 15:28:46 markus Exp $
#	Placed in the Public Domain.

tid="proxy connect"

for p in 1 2; do
	${SSH} -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		fail "ssh proxyconnect protocol $p failed"
	fi
	SSH_CONNECTION=`${SSH} -$p -F $OBJ/ssh_proxy 999.999.999.999 'echo $SSH_CONNECTION'`
	if [ $? -ne 0 ]; then
		fail "ssh proxyconnect protocol $p failed"
	fi
	if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
		fail "bad SSH_CONNECTION"
	fi
done
