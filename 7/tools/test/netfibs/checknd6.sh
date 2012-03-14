#!/bin/sh
#-
# Copyright (c) 2012 Cisco Systems, Inc.
# All rights reserved.
#
# This software was developed by Bjoern Zeeb under contract to
# Cisco Systems, Inc..
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

# We will use the RFC5180 (and Errata) benchmarking working group prefix
# 2001:0002::/48 for testing.
PREFIX="2001:2:"

# Set IFACE to the real interface you want to run the test on.
: ${IFACE:=lo0}

# Number of seconds to wait for peer node to synchronize for test.
: ${WAITS:=120}

# Control port we use to exchange messages between nodes to sync. tests, etc.
: ${CTRLPORT:=6666}

# Get the number of FIBs from the kernel.
RT_NUMFIBS=`sysctl -n net.fibs`

OURADDR="2001:2:ff00::1"
PEERADDR="2001:2:ff00::2"
PEERLINKLOCAL=""

# By default all commands must succeed.  Individual tests may disable this
# temporary.
set -e

# Debug magic.
case "${DEBUG}" in
42)	set -x ;;
esac


################################################################################
#
# Input validation.
#

node=$1
case ${node} in
initiator)	;;
reflector)	;;
*)	echo "ERROR: invalid node name '${node}'. Must be initiator or" \
	    " reflector" >&1
	exit 1
	;;
esac

################################################################################
#
# Helper functions.
#
check_rc()
{
	local _rc _exp _testno _testname _msg _r
	_rc=$1
	_exp=$2
	_testno=$3
	_testname="$4"
	_msg="$5"

	_r="not ok"
	if test ${_rc} -eq ${_exp}; then
		_r="ok"
	fi
	echo "${_r} ${_testno} ${_testname} # ${_msg} ${_rc} ${_exp}"
}

print_debug()
{
	local _msg
	_msg="$*"

	case ${DEBUG} in
	''|0)	;;
	*)	echo "DEBUG: ${_msg}" >&2 ;;
	esac
}

die()
{
	local _msg
	_msg="$*"

	echo "ERROR: ${_msg}" >&2
	exit 1
}

nc_send_recv()
{
	local _loops _msg _expreply _addr _port _opts i
	_loops=$2
	_msg="$3"
	_expreply="$4"
	_addr=$5
	_port=$6
	_opts="$7"

	i=0
	while test ${i} -lt ${_loops}; do
		i=$((i + 1))
		_reply=`echo "${_msg}" | nc -V 0 ${_opts} ${_addr} ${_port}`
		if test "${_reply}" != "${_expreply}"; then
			if test ${i} -lt ${_loops}; then
				sleep 1
			else
			# Must let caller decide how to handle the error.
			#	die "Got invalid reply from peer." \
			#	    "Expected '${_expreply}', got '${_reply}'"
				return 1
			fi
		else
			break
		fi
	done
	return 0
}

send_greeting()
{
        local _type _greeting _keyword _linklocal
	_type="$1"

	set +e
	i=0
	rc=-1
	while test ${i} -lt ${WAITS} -a ${rc} -ne 0; do
		print_debug "Sending greeting #${i} to peer"
		_greeting=`echo "${_type}" | \
		    nc -6 -w 1 ${PEERADDR} ${CTRLPORT}`
		rc=$?
		i=$((i + 1))
		# Might sleep longer in total but better than to DoS
		# and not get anywhere.
		sleep 1
	done
	set -e

	read _keyword _linklocal <<EOI
${_greeting}
EOI
	print_debug "_keyword=${_keyword}"
	print_debug "_linklocal=${_linklocal}"
	case ${_keyword} in
	${_type})	;;
	*)	die "Got invalid keyword in greeting: ${_greeting}"
	;;
	esac
	PEERLINKLOCAL=${_linklocal}

	# Swap the zoneid to the local interface scope.
	PEERLINKLOCAL=${PEERLINKLOCAL%%\%*}"%${IFACE}"

	print_debug "Successfully exchanged greeting. Peer at ${PEERLINKLOCAL}"
}

# We are setup.  Wait for the initiator to tell us that it is ready.
wait_remote_ready()
{
        local _case _msg _keyword
	_case="$1"

	# Wait for the remote to connect and start things.
	# We tell it the magic keyword, and our number of FIBs.
	_msg=`echo "${_case} ${PEERLINKLOCAL}" | nc -6 -l ${CTRLPORT}`

	read _keyword <<EOI
${_msg}
EOI
	print_debug "_keyword=${_keyword}"
	case ${_keyword} in
	${_case});;
	*)	die "Got invalid keyword in control message: ${_msg}"
		;;
	esac

	print_debug "Successfully received control message."
}

setup_addr_initiator()
{
	local i

	print_debug "Setting up interface ${IFACE}"
	ifconfig ${IFACE} inet6 ${OURADDR}/64 alias up
	OURLINKLOCAL=`ifconfig ${IFACE} | awk '/inet6 fe80:/ { print $2 }'`

	i=0
	while test ${i} -lt ${RT_NUMFIBS}; do
		setfib -F ${i} ndp -cn > /dev/null 2>&1
		i=$((i + 1))
	done

	# Let things settle.
	print_debug "Waiting 4 seconds for things to settle"
	sleep 4
}

setup_addr_reflector()
{
	local i

	print_debug "Setting up interface ${IFACE}"
	ifconfig ${IFACE} inet6 ${PEERADDR}/64 alias up
	PEERLINKLOCAL=`ifconfig ${IFACE} | awk '/inet6 fe80:/ { print $2 }'`

	i=0
	while test ${i} -lt ${RT_NUMFIBS}; do
		setfib -F ${i} ndp -cn > /dev/null 2>&1
		i=$((i + 1))
	done

	# Let things settle.
	print_debug "Waiting 4 seconds for things to settle"
	sleep 2
}

cleanup_addr()
{
	local addr
	addr="$1"

	print_debug "Removing address from interface ${IFACE}"
	ifconfig ${IFACE} inet6 ${addr}/64 -alias
}

################################################################################

check_netstat_rn()
{
	local testno fib addr ext c
	testno=$1
	fib=$2
	addr="$3"
	ext="$4"

	c=`netstat -rnf inet6 | egrep "^${addr}[[:space:]]" | wc -l`
	check_rc ${c} 1 ${testno} "check_netstat_addr_${fib}_${ext}" \
	    "FIB ${fib} ${addr}"
	if test ${c} -ne 1; then
		print_debug "`netstat -rnf inet6`"
	fi
}

check_ndp()
{
	local testno fib addr ext c
	testno=$1
	fib=$2
	addr="$3"
	ext="$4"

	# Check that the entry is there and 'R'eachable still.
	c=`ndp -an | egrep "^${addr}[[:space:]].*R" | wc -l`
	check_rc ${c} 1 ${testno} "check_ndp_addr_${fib}_${ext}" \
	    "FIB ${fib} ${addr}"
	if test ${c} -ne 1; then
		print_debug "`ndp -an`"
	fi
}

check_nd6()
{
	local testno fib

	printf "1..%d\n" `expr 6 \* ${RT_NUMFIBS}`
	testno=1
	fib=0
	set +e
	while test ${fib} -lt ${RT_NUMFIBS}; do
		print_debug "Testing FIB ${fib}"

		setfib -F${fib} ping6 -n -c3 ${PEERLINKLOCAL} > /dev/null 2>&1
		check_rc $? 0 ${testno} "check_local_addr_${fib}_l" \
		    "FIB ${fib} ${_l}"
		testno=$((testno + 1))

		check_netstat_rn ${testno} ${fib} ${PEERLINKLOCAL} "l"
		testno=$((testno + 1))

		check_ndp ${testno} ${fib} ${PEERLINKLOCAL} "l"
		testno=$((testno + 1))

		setfib -F${fib} ping6 -n -c3 ${PEERADDR} > /dev/null 2>&1
		check_rc $? 0 ${testno} "check_local_addr_${fib}_a" \
		    "FIB ${fib} ${OURADDR}"
		testno=$((testno + 1))

		check_netstat_rn ${testno} ${fib} ${PEERADDR} "a"
		testno=$((testno + 1))

		check_ndp ${testno} ${fib} ${PEERADDR} "a"
		testno=$((testno + 1))

		fib=$((fib + 1))
	done
	set -e
}

################################################################################
#
# MAIN
#

case ${node} in
initiator)
	setup_addr_initiator
	send_greeting BEGIN
	check_nd6
	send_greeting END
	cleanup_addr ${OURADDR}
	;;
reflector)
	setup_addr_reflector
	wait_remote_ready BEGIN
	wait_remote_ready END
	cleanup_addr ${PEERADDR}
	;;
esac

# end
