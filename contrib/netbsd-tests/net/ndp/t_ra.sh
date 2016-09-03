#	$NetBSD: t_ra.sh,v 1.3 2016/08/10 23:07:57 kre Exp $
#
# Copyright (c) 2015 Internet Initiative Japan Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

RUMPFLAGS="-lrumpnet -lrumpnet_net -lrumpnet_netinet -lrumpnet_netinet6"
RUMPFLAGS="${RUMPFLAGS} -lrumpnet_shmif -lrumpdev"
RUMPFLAGS="${RUMPFLAGS} -lrumpvfs -lrumpfs_ffs"

RUMPSRV=unix://r1
RUMPCLI=unix://r2
IP6SRV=fc00:1::1
IP6CLI=fc00:2::2
PIDFILE=/var/run/rump.rtadvd.pid
CONFIG=./rtadvd.conf
DEBUG=true

setup_shmif0()
{
	local IP6ADDR=${1}
	shift

	atf_check -s exit:0 rump.ifconfig shmif0 create
	atf_check -s exit:0 rump.ifconfig shmif0 linkstr bus1
	atf_check -s exit:0 rump.ifconfig shmif0 inet6 ${IP6ADDR}
	atf_check -s exit:0 rump.ifconfig shmif0 up

	$DEBUG && rump.ifconfig
}

wait_term()
{
	local PIDFILE=${1}
	shift

	while [ -f ${PIDFILE} ]
	do
		sleep 0.2
	done

	return 0
}

create_rtadvdconfig()
{

	cat << _EOF > ${CONFIG}
shmif0:\
	:mtu#1300:maxinterval#4:mininterval#3:
_EOF
}

atf_test_case basic cleanup
basic_head()
{

	atf_set "descr" "Tests for basic functions of router advaertisement(RA)"
	atf_set "require.progs" "rump_server rump.rtadvd rump.ndp rump.ifconfig"
}

basic_body()
{

	atf_check -s exit:0 rump_server ${RUMPFLAGS} ${RUMPSRV}
	atf_check -s exit:0 rump_server ${RUMPFLAGS} ${RUMPCLI}

	export RUMP_SERVER=${RUMPSRV}
	setup_shmif0 ${IP6SRV}
	atf_check -s exit:0 -o match:'0.->.1' rump.sysctl -w net.inet6.ip6.forwarding=1
	export LD_PRELOAD=/usr/lib/librumphijack.so
	atf_check -s exit:0 mkdir -p /rump/var/chroot/rtadvd
	unset LD_PRELOAD
	unset RUMP_SERVER

	export RUMP_SERVER=${RUMPCLI}
	setup_shmif0 ${IP6CLI}
	$DEBUG && rump.ndp -n -a
	atf_check -s exit:0 -o match:'= 0' rump.sysctl net.inet6.ip6.accept_rtadv
	unset RUMP_SERVER

	create_rtadvdconfig

	export RUMP_SERVER=${RUMPSRV}
	atf_check -s exit:0 rump.rtadvd -c ${CONFIG} shmif0
	atf_check -s exit:0 sleep 3
	atf_check -s exit:0 -o ignore -e empty cat ${PIDFILE}
	unset RUMP_SERVER

	export RUMP_SERVER=${RUMPCLI}
	atf_check -s exit:0 -o empty rump.ndp -r
	atf_check -s exit:0 -o not-match:'advertised' rump.ndp -p
	atf_check -s exit:0 -o match:'linkmtu=0' rump.ndp -n -i shmif0
	atf_check -s exit:0 -o not-match:'S R' rump.ndp -n -a
	atf_check -s exit:0 -o not-match:'fc00:1:' rump.ndp -n -a
	atf_check -s exit:0 -o not-match:'fc00:1:' rump.ifconfig shmif0 inet6
	unset RUMP_SERVER

	atf_check -s exit:0 kill -TERM `cat ${PIDFILE}`
	wait_term ${PIDFILE}

	export RUMP_SERVER=${RUMPCLI}
	atf_check -s exit:0 -o match:'0.->.1' rump.sysctl -w net.inet6.ip6.accept_rtadv=1
	unset RUMP_SERVER

	export RUMP_SERVER=${RUMPSRV}
	atf_check -s exit:0 rump.rtadvd -c ${CONFIG} shmif0
	atf_check -s exit:0 sleep 3
	atf_check -s exit:0 -o ignore -e empty cat ${PIDFILE}
	unset RUMP_SERVER

	export RUMP_SERVER=${RUMPCLI}
	$DEBUG && rump.ndp -n -a
	$DEBUG && rump.ndp -r
	atf_check -s exit:0 -o match:'if=shmif0' rump.ndp -r
	atf_check -s exit:0 -o match:'advertised' rump.ndp -p
	atf_check -s exit:0 -o match:'linkmtu=1300' rump.ndp -n -i shmif0
	atf_check -s exit:0 -o match:'23h59m..s S R' rump.ndp -n -a
	atf_check -s exit:0 -o match:'fc00:1:' rump.ndp -n -a
	atf_check -s exit:0 -o match:'fc00:1:' rump.ifconfig shmif0 inet6
	unset RUMP_SERVER

	atf_check -s exit:0 kill -TERM `cat ${PIDFILE}`
	wait_term ${PIDFILE}

	return 0
}

basic_cleanup()
{

	if [ -f ${PIDFILE} ]; then
		kill -TERM `cat ${PIDFILE}`
		wait_term ${PIDFILE}
	fi

	env RUMP_SERVER=${RUMPSRV} rump.halt
	env RUMP_SERVER=${RUMPCLI} rump.halt
}

atf_init_test_cases()
{

	atf_add_test_case basic
}
