#	$OpenBSD: connect.sh,v 1.5 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="simple connect"

start_sshd

for p in ${SSH_PROTOCOLS}; do
	${SSH} -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed"
	fi
done
