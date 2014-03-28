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


# Regression test for bin/187551
atf_test_case process_fib cleanup
process_fib_head()
{
	atf_set "descr" "ifconfig will set its process fib whenever configuring an interface with nondefault fib"
	atf_set "require.user" "root"
	atf_set "require.config" "fibs"
}
process_fib_body()
{
	atf_expect_fail "bin/187551 ifconfig should change its process fib when configuring an interface with nondefault fib"
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
	get_tap
	ktrace ifconfig $TAP ${ADDR}/${MASK} fib $FIB0
	if kdump -s | egrep -q 'CALL[[:space:]]+setfib'; then
		atf_pass
	else
		atf_fail "ifconfig never called setfib(2)"
	fi
}

process_fib_cleanup()
{
	cleanup_tap
}

atf_init_test_cases()
{
	atf_add_test_case process_fib
}


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




cleanup_tap()
{
	for TAPD in `cat "tap_devices_to_cleanup"`; do
		ifconfig ${TAPD} destroy
	done
}

