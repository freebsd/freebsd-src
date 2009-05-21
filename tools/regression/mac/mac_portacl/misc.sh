#!/bin/sh
# $FreeBSD$

sysctl security.mac.portacl >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "1..1"
	echo "not ok 1 # MAC_PORTACL is unavailable."
	exit 0
fi

ntest=1

check_bind() {
	idtype=${1}
	name=${2}
	proto=${3}
	port=${4}

	[ "${proto}" = "udp" ] && udpflag="-u"

	out=`(
		case "${idtype}" in
		uid|gid)
			( echo -n | su -m ${name} -c "nc ${udpflag} -o -l 127.0.0.1 $port" 2>&1 ) &
			;;
		jail)
			kill $$
			;;
		*)
			kill $$
		esac
		sleep 0.3
		echo | nc ${udpflag} -o 127.0.0.1 $port >/dev/null 2>&1
		wait
	)`
	case "${out}" in
	"nc: Permission denied"*|"nc: Operation not permitted"*)
		echo fl
		;;
	"")
		echo ok
		;;
	*)
		echo ${out}
		;;
	esac
}

bind_test() {
	expect_without_rule=${1}
	expect_with_rule=${2}
	idtype=${3}
	name=${4}
	proto=${5}
	port=${6}

	sysctl security.mac.portacl.rules= >/dev/null
	out=`check_bind ${idtype} ${name} ${proto} ${port}`
	if [ "${out}" = "${expect_without_rule}" ]; then
		echo "ok ${ntest}"
	elif [ "${out}" = "ok" -o "${out}" = "fl" ]; then
		echo "not ok ${ntest}"
	else
		echo "not ok ${ntest} # ${out}"
	fi
	ntest=$((ntest+1))

	if [ "${idtype}" = "uid" ]; then
		idstr=`id -u ${name}`
	elif [ "${idtype}" = "gid" ]; then
		idstr=`id -g ${name}`
	else
		idstr=${name}
	fi
	sysctl security.mac.portacl.rules=${idtype}:${idstr}:${proto}:${port} >/dev/null
	out=`check_bind ${idtype} ${name} ${proto} ${port}`
	if [ "${out}" = "${expect_with_rule}" ]; then
		echo "ok ${ntest}"
	elif [ "${out}" = "ok" -o "${out}" = "fl" ]; then
		echo "not ok ${ntest}"
	else
		echo "not ok ${ntest} # ${out}"
	fi
	ntest=$((ntest+1))

	sysctl security.mac.portacl.rules= >/dev/null
}

reserved_high=`sysctl -n net.inet.ip.portrange.reservedhigh`
suser_exempt=`sysctl -n security.mac.portacl.suser_exempt`
port_high=`sysctl -n security.mac.portacl.port_high`

restore_settings() {
	sysctl -n net.inet.ip.portrange.reservedhigh=${reserved_high} >/dev/null
	sysctl -n security.mac.portacl.suser_exempt=${suser_exempt} >/dev/null
	sysctl -n security.mac.portacl.port_high=${port_high} >/dev/null
}
