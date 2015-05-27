#
#  Copyright (c) 2014 Spectra Logic Corporation
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions, and the following disclaimer,
#     without modification.
#  2. Redistributions in binary form must reproduce at minimum a disclaimer
#     substantially similar to the "NO WARRANTY" disclaimer below
#     ("Disclaimer") and any redistribution must be conditioned upon
#     including a substantially similar Disclaimer requirement for further
#     binary redistribution.
# 
#  NO WARRANTY
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
#  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
#  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGES.
# 
#  Authors: Alan Somers         (Spectra Logic Corporation)
#
# $FreeBSD$

atf_test_case static_ipv6_loopback_route_for_each_fib cleanup
static_ipv6_loopback_route_for_each_fib_head()
{
	atf_set "descr" "Every FIB should have a static IPv6 loopback route"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
	atf_set "require.progs" "sysrc"
}
static_ipv6_loopback_route_for_each_fib_body()
{
	# Configure the TAP interface to use an RFC5737 nonrouteable address
	# and a non-default fib
	ADDR="192.0.2.2"
	SUBNET="192.0.2.0"
	MASK="24"

	# Check system configuration
	if [ 0 != `sysctl -n net.add_addr_allfibs` ]; then
		atf_skip "This test requires net.add_addr_allfibs=0"
	fi

	get_fibs 1
	get_tap
	
	# Configure a TAP interface in /etc/rc.conf.  Register the sysrc
	# variable for cleanup.
	echo "ifconfig_${TAP}" >> "sysrc_vars_to_cleanup"
	sysrc ifconfig_${TAP}="${ADDR}/${MASK} fib ${FIB0}"

	# Start the interface
	service netif start ${TAP}
	# Check for an IPv6 loopback route
	setfib ${FIB0} netstat -rn -f inet6 | grep -q "^::1.*lo0$"
	if [ 0 -eq $? ]; then
		atf_pass
	else
		setfib ${FIB0} netstat -rn -f inet6
		atf_fail "Did not find an IPv6 loopback route"
	fi
}
static_ipv6_loopback_route_for_each_fib_cleanup()
{
	cleanup_sysrc
	cleanup_tap
}

atf_init_test_cases()
{
	atf_add_test_case static_ipv6_loopback_route_for_each_fib
}

# Looks up one or more fibs from the configuration data and validates them.
# Returns the results in the env varilables FIB0, FIB1, etc.
# parameter numfibs	The number of fibs to lookup
get_fibs()
{
	NUMFIBS=$1
	net_fibs=`sysctl -n net.fibs`
	i=0
	while [ $i -lt "$NUMFIBS" ]; do
		fib=`atf_config_get "fibs" | \
			awk -v i=$(( i + 1 )) '{print $i}'`
		echo "fib is ${fib}"
		eval FIB${i}=${fib}
		if [ "$fib" -ge "$net_fibs" ]; then
			msg="The ${i}th configured fib is ${fub}, which is "
			msg="$msg not less than net.fibs (${net_fibs})"
			atf_skip "$msg"
		fi
		i=$(( $i + 1 ))
	done
}


# Creates a new tap(4) interface, registers it for cleanup, and returns the
# name via the environment variable TAP
get_tap()
{
	local TAPN=0
	while ! ifconfig tap${TAPN} create > /dev/null 2>&1; do
		if [ "$TAPN" -ge 8 ]; then
			atf_skip "Could not create a tap(4) interface"
		else
			TAPN=$(($TAPN + 1))
		fi
	done
	local TAPD=tap${TAPN}
	# Record the TAP device so we can clean it up later
	echo ${TAPD} >> "tap_devices_to_cleanup"
	TAP=${TAPD}
}

cleanup_sysrc()
{
	for var in `cat "sysrc_vars_to_cleanup"`; do
		sysrc -x $var
	done
}

cleanup_tap()
{
	for TAPD in `cat "tap_devices_to_cleanup"`; do
		ifconfig ${TAPD} destroy
	done
}
