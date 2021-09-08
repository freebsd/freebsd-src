#	$OpenBSD: connect.sh,v 1.8 2020/01/25 02:57:53 dtucker Exp $
#	Placed in the Public Domain.

tid="simple connect"

start_sshd

trace "direct connect"
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh direct connect failed"
fi

trace "proxy connect"
${SSH} -F $OBJ/ssh_config -o "proxycommand $NC %h %p" somehost true
if [ $? -ne 0 ]; then
	fail "ssh proxycommand connect failed"
fi
