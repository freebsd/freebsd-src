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

# All of the tests in this file requires the test-suite config variable "fibs"
# to be defined to a space-delimited list of FIBs that may be used for testing.

# arpresolve should check the interface fib for routes to a target when
# creating an ARP table entry.  This is a regression for kern/167947, where
# arpresolve only checked the default route.
#
# Outline:
# Create two tap(4) interfaces
# Simulate a crossover cable between them by using net/socat
# Use nping (from security/nmap) to send an ICMP echo request from one
# interface to the other, spoofing the source IP.  The source IP must be
# spoofed, or else it will already have an entry in the arp table.
# Check whether an arp entry exists for the spoofed IP
atf_test_case arpresolve_checks_interface_fib cleanup
arpresolve_checks_interface_fib_head()
{
	atf_set "descr" "arpresolve should check the interface fib, not the default fib, for routes"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
	atf_set "require.progs" "socat nping"
}
arpresolve_checks_interface_fib_body()
{
	# Configure the TAP interfaces to use a RFC5737 nonrouteable addresses
	# and a non-default fib
	ADDR0="192.0.2.2"
	ADDR1="192.0.2.3"
	SUBNET="192.0.2.0"
	# Due to bug TBD (regressed by multiple_fibs_on_same_subnet) we need
	# diffferent subnet masks, or FIB1 won't have a subnet route.
	MASK0="24"
	MASK1="25"
	# Spoof a MAC that is reserved per RFC7042
	SPOOF_ADDR="192.0.2.4"
	SPOOF_MAC="00:00:5E:00:53:00"

	# Check system configuration
	if [ 0 != `sysctl -n net.add_addr_allfibs` ]; then
		atf_skip "This test requires net.add_addr_allfibs=0"
	fi
	get_fibs 2

	# Configure TAP interfaces
	setup_tap "$FIB0" ${ADDR0} ${MASK0}
	TAP0=$TAP
	setup_tap "$FIB1" ${ADDR1} ${MASK1}
	TAP1=$TAP

	# Simulate a crossover cable
	socat /dev/${TAP0} /dev/${TAP1} &
	SOCAT_PID=$!
	echo ${SOCAT_PID} >> "processes_to_kill"

	# Send an ICMP echo request with a spoofed source IP
	setfib 2 nping -c 1 -e ${TAP0} -S ${SPOOF_ADDR} \
		--source-mac ${SPOOF_MAC} --icmp --icmp-type "echo-request" \
		--icmp-code 0 --icmp-id 0xdead --icmp-seq 1 --data 0xbeef \
		${ADDR1}
	# For informational and debugging purposes only, look for the
	# characteristic error message
	dmesg | grep "llinfo.*${SPOOF_ADDR}"
	# Check that the ARP entry exists
	atf_check -o match:"${SPOOF_ADDR}.*expires" setfib 3 arp ${SPOOF_ADDR}
}
arpresolve_checks_interface_fib_cleanup()
{
	if [ -f processes_to_kill ]; then
		for pid in $(cat processes_to_kill); do
			kill "${pid}"
		done
		rm -f processes_to_kill
	fi
	cleanup_tap
}


# Regression test for kern/187549
atf_test_case loopback_and_network_routes_on_nondefault_fib cleanup
loopback_and_network_routes_on_nondefault_fib_head()
{
	atf_set "descr" "When creating and deleting loopback routes, use the interface's fib"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}

loopback_and_network_routes_on_nondefault_fib_body()
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

	# Configure a TAP interface
	setup_tap ${FIB0} ${ADDR} ${MASK}

	# Check whether the host route exists in only the correct FIB
	setfib ${FIB0} netstat -rn -f inet | grep -q "^${ADDR}.*UHS.*lo0"
	if [ 0 -ne $? ]; then
		setfib ${FIB0} netstat -rn -f inet
		atf_fail "Host route did not appear in the correct FIB"
	fi
	setfib 0 netstat -rn -f inet | grep -q "^${ADDR}.*UHS.*lo0"
	if [ 0 -eq $? ]; then
		setfib 0 netstat -rn -f inet
		atf_fail "Host route appeared in the wrong FIB"
	fi

	# Check whether the network route exists in only the correct FIB
	setfib ${FIB0} netstat -rn -f inet | \
		grep -q "^${SUBNET}/${MASK}.*${TAPD}"
	if [ 0 -ne $? ]; then
		setfib ${FIB0} netstat -rn -f inet
		atf_fail "Network route did not appear in the correct FIB"
	fi
	setfib 0 netstat -rn -f inet | \
		grep -q "^${SUBNET}/${MASK}.*${TAPD}"
	if [ 0 -eq $? ]; then
		setfib ${FIB0} netstat -rn -f inet
		atf_fail "Network route appeared in the wrong FIB"
	fi
}

loopback_and_network_routes_on_nondefault_fib_cleanup()
{
	cleanup_tap
}


# Regression test for kern/187552
atf_test_case default_route_with_multiple_fibs_on_same_subnet cleanup
default_route_with_multiple_fibs_on_same_subnet_head()
{
	atf_set "descr" "Multiple interfaces on the same subnet but with different fibs can both have default routes"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}

default_route_with_multiple_fibs_on_same_subnet_body()
{
	# Configure the TAP interfaces to use a RFC5737 nonrouteable addresses
	# and a non-default fib
	ADDR0="192.0.2.2"
	ADDR1="192.0.2.3"
	GATEWAY="192.0.2.1"
	SUBNET="192.0.2.0"
	MASK="24"

	# Check system configuration
	if [ 0 != `sysctl -n net.add_addr_allfibs` ]; then
		atf_skip "This test requires net.add_addr_allfibs=0"
	fi
	get_fibs 2

	# Configure TAP interfaces
	setup_tap "$FIB0" ${ADDR0} ${MASK}
	TAP0=$TAP
	setup_tap "$FIB1" ${ADDR1} ${MASK}
	TAP1=$TAP

	# Attempt to add default routes
	setfib ${FIB0} route add default ${GATEWAY}
	setfib ${FIB1} route add default ${GATEWAY}

	# Verify that the default route exists for both fibs, with their
	# respective interfaces.
	atf_check -o match:"^default.*${TAP0}$" \
		setfib ${FIB0} netstat -rn -f inet
	atf_check -o match:"^default.*${TAP1}$" \
		setfib ${FIB1} netstat -rn -f inet
}

default_route_with_multiple_fibs_on_same_subnet_cleanup()
{
	cleanup_tap
}


# Regression test for PR kern/189089
# Create two tap interfaces and assign them both the same IP address but with
# different netmasks, and both on the default FIB.  Then remove one's IP
# address.  Hopefully the machine won't panic.
atf_test_case same_ip_multiple_ifaces_fib0 cleanup
same_ip_multiple_ifaces_fib0_head()
{
	atf_set "descr" "Can remove an IP alias from an interface when the same IP is also assigned to another interface."
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}
same_ip_multiple_ifaces_fib0_body()
{
	ADDR="192.0.2.2"
	MASK0="24"
	MASK1="32"

	# Unlike most of the tests in this file, this is applicable regardless
	# of net.add_addr_allfibs

	# Setup the interfaces, then remove one alias.  It should not panic.
	setup_tap 0 ${ADDR} ${MASK0}
	TAP0=${TAP}
	setup_tap 0 ${ADDR} ${MASK1}
	TAP1=${TAP}
	ifconfig ${TAP1} -alias ${ADDR}

	# Do it again, in the opposite order.  It should not panic.
	setup_tap 0 ${ADDR} ${MASK0}
	TAP0=${TAP}
	setup_tap 0 ${ADDR} ${MASK1}
	TAP1=${TAP}
	ifconfig ${TAP0} -alias ${ADDR}
}
same_ip_multiple_ifaces_fib0_cleanup()
{
	cleanup_tap
}

# Regression test for PR kern/189088
# Test that removing an IP address works even if the same IP is assigned to a
# different interface, on a different FIB.  Tests the same code that whose
# panic was regressed by same_ip_multiple_ifaces_fib0.  
# Create two tap interfaces and assign them both the same IP address but with
# different netmasks, and on different FIBs.  Then remove one's IP
# address.  Hopefully the machine won't panic.  Also, the IP's hostroute should
# dissappear from the correct fib.
atf_test_case same_ip_multiple_ifaces cleanup
same_ip_multiple_ifaces_head()
{
	atf_set "descr" "Can remove an IP alias from an interface when the same IP is also assigned to another interface, on non-default FIBs."
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}
same_ip_multiple_ifaces_body()
{
	atf_expect_fail "kern/189088 Assigning the same IP to multiple interfaces in different FIBs creates a host route for only one"
	ADDR="192.0.2.2"
	MASK0="24"
	MASK1="32"

	# Unlike most of the tests in this file, this is applicable regardless
	# of net.add_addr_allfibs
	get_fibs 2

	# Setup the interfaces, then remove one alias.  It should not panic.
	setup_tap ${FIB0} ${ADDR} ${MASK0}
	TAP0=${TAP}
	setup_tap ${FIB1} ${ADDR} ${MASK1}
	TAP1=${TAP}
	ifconfig ${TAP1} -alias ${ADDR}
	atf_check -o not-match:"^${ADDR}[[:space:]]" \
		setfib ${FIB1} netstat -rn -f inet

	# Do it again, in the opposite order.  It should not panic.
	setup_tap ${FIB0} ${ADDR} ${MASK0}
	TAP0=${TAP}
	setup_tap ${FIB1} ${ADDR} ${MASK1}
	TAP1=${TAP}
	ifconfig ${TAP0} -alias ${ADDR}
	atf_check -o not-match:"^${ADDR}[[:space:]]" \
		setfib ${FIB0} netstat -rn -f inet
}
same_ip_multiple_ifaces_cleanup()
{
	# Due to PR kern/189088, we must destroy the interfaces in LIFO order
	# in order for the routes to be correctly cleaned up.
	for TAPD in `tail -r "tap_devices_to_cleanup"`; do
		ifconfig ${TAPD} destroy
	done
}

# Regression test for kern/187550
atf_test_case subnet_route_with_multiple_fibs_on_same_subnet cleanup
subnet_route_with_multiple_fibs_on_same_subnet_head()
{
	atf_set "descr" "Multiple FIBs can have subnet routes for the same subnet"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}

subnet_route_with_multiple_fibs_on_same_subnet_body()
{
	# Configure the TAP interfaces to use a RFC5737 nonrouteable addresses
	# and a non-default fib
	ADDR0="192.0.2.2"
	ADDR1="192.0.2.3"
	SUBNET="192.0.2.0"
	MASK="24"

	# Check system configuration
	if [ 0 != `sysctl -n net.add_addr_allfibs` ]; then
		atf_skip "This test requires net.add_addr_allfibs=0"
	fi
	get_fibs 2

	# Configure TAP interfaces
	setup_tap "$FIB0" ${ADDR0} ${MASK}
	setup_tap "$FIB1" ${ADDR1} ${MASK}

	# Check that a subnet route exists on both fibs
	atf_check -o ignore setfib "$FIB0" route get $ADDR1
	atf_check -o ignore setfib "$FIB1" route get $ADDR0
}

subnet_route_with_multiple_fibs_on_same_subnet_cleanup()
{
	cleanup_tap
}

# Test that source address selection works correctly for UDP packets with
# SO_DONTROUTE set that are sent on non-default FIBs.
# This bug was discovered with "setfib 1 netperf -t UDP_STREAM -H some_host"
# Regression test for kern/187553
#
# The root cause was that ifa_ifwithnet() did not have a fib argument.  It
# would return an address from an interface on any FIB that had a subnet route
# for the destination.  If more than one were available, it would choose the
# most specific.  This is most easily tested by creating a FIB without a
# default route, then trying to send a UDP packet with SO_DONTROUTE set to an
# address which is not routable on that FIB.  Absent the fix for this bug,
# in_pcbladdr would choose an interface on any FIB with a default route.  With
# the fix, you will get EUNREACH or ENETUNREACH.
atf_test_case udp_dontroute cleanup
udp_dontroute_head()
{
	atf_set "descr" "Source address selection for UDP packets with SO_DONTROUTE on non-default FIBs works"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}

udp_dontroute_body()
{
	# Configure the TAP interface to use an RFC5737 nonrouteable address
	# and a non-default fib
	ADDR0="192.0.2.2"
	ADDR1="192.0.2.3"
	SUBNET="192.0.2.0"
	MASK="24"
	# Use a different IP on the same subnet as the target
	TARGET="192.0.2.100"
	SRCDIR=`atf_get_srcdir`

	# Check system configuration
	if [ 0 != `sysctl -n net.add_addr_allfibs` ]; then
		atf_skip "This test requires net.add_addr_allfibs=0"
	fi
	get_fibs 2

	# Configure the TAP interfaces
	setup_tap ${FIB0} ${ADDR0} ${MASK}
	TARGET_TAP=${TAP}
	setup_tap ${FIB1} ${ADDR1} ${MASK}

	# Send a UDP packet with SO_DONTROUTE.  In the failure case, it will
	# return ENETUNREACH, or send the packet to the wrong tap
	atf_check -o ignore setfib ${FIB0} \
		${SRCDIR}/udp_dontroute ${TARGET} /dev/${TARGET_TAP}
	cleanup_tap

	# Repeat, but this time target the other tap
	setup_tap ${FIB0} ${ADDR0} ${MASK}
	setup_tap ${FIB1} ${ADDR1} ${MASK}
	TARGET_TAP=${TAP}

	atf_check -o ignore setfib ${FIB1} \
		${SRCDIR}/udp_dontroute ${TARGET} /dev/${TARGET_TAP}
}

udp_dontroute_cleanup()
{
	cleanup_tap
}


atf_init_test_cases()
{
	atf_add_test_case arpresolve_checks_interface_fib
	atf_add_test_case loopback_and_network_routes_on_nondefault_fib
	atf_add_test_case default_route_with_multiple_fibs_on_same_subnet
	atf_add_test_case same_ip_multiple_ifaces_fib0
	atf_add_test_case same_ip_multiple_ifaces
	atf_add_test_case subnet_route_with_multiple_fibs_on_same_subnet
	atf_add_test_case udp_dontroute
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
			atf_skip "The ${i}th configured fib is ${fib}, which is not less than net.fibs, which is ${net_fibs}"
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

# Create a tap(4) interface, configure it, and register it for cleanup.
# parameters:
# fib
# IP address
# Netmask in number of bits (eg 24 or 8)
# Return: the tap interface name as the env variable TAP
setup_tap()
{
	local FIB=$1
	local ADDR=$2
	local MASK=$3
	get_tap
	echo setfib ${FIB} ifconfig $TAP ${ADDR}/${MASK} fib $FIB
	setfib ${FIB} ifconfig $TAP ${ADDR}/${MASK} fib $FIB
}

cleanup_tap()
{
	if [ -f tap_devices_to_cleanup ]; then
		for tap_device in $(cat tap_devices_to_cleanup); do
			ifconfig "${tap_device}" destroy
		done
		rm -f tap_devices_to_cleanup
	fi
}
