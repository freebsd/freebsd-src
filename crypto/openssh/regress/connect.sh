#	$OpenBSD: connect.sh,v 1.4 2002/03/15 13:08:56 markus Exp $
#	Placed in the Public Domain.

tid="simple connect"

start_sshd

for p in 1 2; do
	${SSH} -o "Protocol=$p" -F $OBJ/ssh_config somehost true
	if [ $? -ne 0 ]; then
		fail "ssh connect with protocol $p failed"
	fi
done
