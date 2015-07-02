#	$OpenBSD: proto-version.sh,v 1.5 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="sshd version with different protocol combinations"

# we just start sshd in inetd mode and check the banner
check_version ()
{
	version=$1
	expect=$2
	banner=`printf '' | ${SSHD} -o "Protocol=${version}" -i -f ${OBJ}/sshd_proxy`
	case ${banner} in
	SSH-1.99-*)
		proto=199
		;;
	SSH-2.0-*)
		proto=20
		;;
	SSH-1.5-*)
		proto=15
		;;
	*)
		proto=0
		;;
	esac
	if [ ${expect} -ne ${proto} ]; then
		fail "wrong protocol version ${banner} for ${version}"
	fi
}

check_version	2	20
if ssh_version 1; then
	check_version	2,1	199
	check_version	1,2	199
	check_version	1	15
fi
