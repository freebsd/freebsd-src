#	$OpenBSD: proto-mismatch.sh,v 1.3 2002/03/15 13:08:56 markus Exp $
#	Placed in the Public Domain.

tid="protocol version mismatch"

mismatch ()
{
	server=$1
	client=$2
	banner=`echo ${client} | ${SSHD} -o "Protocol=${server}" -i -f ${OBJ}/sshd_proxy`
	r=$?
	trace "sshd prints ${banner}"
	if [ $r -ne 255 ]; then
		fail "sshd prints ${banner} and accepts connect with version ${client}"
	fi
}

mismatch	2	SSH-1.5-HALLO
mismatch	1	SSH-2.0-HALLO
