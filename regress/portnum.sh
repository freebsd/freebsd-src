#	$OpenBSD: portnum.sh,v 1.1 2009/08/13 00:57:17 djm Exp $
#	Placed in the Public Domain.

tid="port number parsing"

badport() {
	port=$1
	verbose "$tid: invalid port $port"
	if ${SSH} -F $OBJ/ssh_proxy -p $port somehost true 2>/dev/null ; then
		fail "$tid accepted invalid port $port"
	fi
}
goodport() {
	port=$1
	verbose "$tid: valid port $port"
	if ${SSH} -F $OBJ/ssh_proxy -p $port somehost true 2>/dev/null ; then
		:
	else
		fail "$tid rejected valid port $port"
	fi
}

badport 0
badport 65536
badport 131073
badport 2000blah
badport blah2000

goodport 1
goodport 22
goodport 2222
goodport 22222
goodport 65535

