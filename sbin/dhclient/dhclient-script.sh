#!/bin/sh

#############################################################################
#
# Copyright (c) 1999, MindStep Corporation
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
#
#############################################################################
#
# This script was written by Patrick Bihan-Faou, patrick@mindstep.com,
# Please contact us for bug reports, etc.
#
#############################################################################
# $MindStep_Id: dhclient-script.sh,v 1.8 1999/12/07 22:11:08 patrick Exp $
# $MindStep_Tag: CONTRIB_19991207 $
# $FreeBSD$
#############################################################################


#############################################################################
# hook functions prototypes
#
# The "pre_state_XXX_hook" functions are called before the main
# work is done for the state XXX
#
# The "post_state_XXX_hook" functions are called after the main
# work is done for the state XXX
#
# These functions are meant to be overridden by the user's
# dhclient-enter-hooks file
#############################################################################

pre_state_MEDIUM_hook () { }
pre_state_PREINIT_hook () { }
pre_state_ARPCHECK_hook () { }
pre_state_ARPSEND_hook () { }
pre_state_RENEW_hook () { }
pre_state_REBIND_hook () { }
pre_state_BOUND_hook () { }
pre_state_REBOOT_hook () { }
pre_state_EXPIRE_hook () { }
pre_state_FAIL_hook () { }
pre_state_TIMEOUT_hook () { }
post_state_MEDIUM_hook () { }
post_state_PREINIT_hook () { }
post_state_ARPCHECK_hook () { }
post_state_ARPSEND_hook () { }
post_state_RENEW_hook () { }
post_state_REBIND_hook () { }
post_state_BOUND_hook () { }
post_state_REBOOT_hook () { }
post_state_EXPIRE_hook () { }
post_state_FAIL_hook () { }
post_state_TIMEOUT_hook () { }

#############################################################################
# make_resolv_conf
#
# This function is called to update the information related to the
# DNS configuration (the resolver part)
#############################################################################
make_resolv_conf () 
{
   if [ "x$new_domain_name" != x ] && [ "x$new_domain_name_servers" != x ]; then
     echo search $new_domain_name >/etc/resolv.conf
     for nameserver in $new_domain_name_servers; do
       echo nameserver $nameserver >>/etc/resolv.conf
     done
   fi
}

# Must be used on exit.   Invokes the local dhcp client exit hooks, if any.
exit_with_hooks () {
  exit_status=$1
  if [ -x /etc/dhclient-exit-hooks ]; then
    . /etc/dhclient-exit-hooks
  fi
# probably should do something with exit status of the local script
  return $exit_status
}

#############################################################################
# set_XXX
# unset_XXX
#
# These function each deal with one particular setting.
# They are OS dependent and may be overridden in the 
# dhclient-enter-hooks file if needed.
#
# These functions are called with either "new" or "old" to indicate which
# set of variables to use (new_ip_address or old_ip_address...)
#
#############################################################################

update_hostname ()
{
	local current_hostname=`/bin/hostname`
  	if	[ "$current_hostname" = "" ] || \
		[ "$current_hostname" = "$old_host_name" ]
	then
		if [ "$new_host_name" != "$old_host_name" ]
		then
			$LOGGER "New Hostname: $new_host_name"
			hostname $new_host_name
		fi
	fi
}

set_ip_address () 
{
	local ip
	local mask
	local bcast

	if [ $# -lt 1 ]
	then
		return  1
	fi

	eval ip="\$${1}_ip_address"
	eval mask="\$${1}_subnet_mask"
	eval bcast="\$${1}_broadcast_address"

	if [ "$ip" != "" ]
	then
		ifconfig $interface inet $ip netmask $mask broadcast $bcast $medium
#		route add $ip 127.0.0.1 > /dev/null 2>&1
	fi
}

unset_ip_address () 
{
	local ip

	if [ $# -lt 1 ]
	then
		return  1
	fi

	eval ip="\$${1}_ip_address"

	if [ "$ip" != "" ]
	then
		ifconfig $interface inet -alias $ip $medium
#		route delete $ip 127.0.0.1 > /dev/null 2>&1
	fi
}

set_ip_alias () 
{
	if [ "$alias_ip_address" != "" ]
	then
		ifconfig $interface inet alias $alias_ip_address netmask $alias_subnet_mask
#		route add $alias_ip_address 127.0.0.1
	fi
}

unset_ip_alias () 
{
	if [ "$alias_ip_address" != "" ]
	then
		ifconfig $interface inet -alias $alias_ip_address > /dev/null 2>&1
#		route delete $alias_ip_address 127.0.0.1 > /dev/null 2>&1
	fi
}

set_routers () 
{
	local router_list

	if [ $# -lt 1 ]
	then
		return  1
	fi

	eval router_list="\$${1}_routers"

	for router in $router_list
	do
		route add default $router >/dev/null 2>&1
	done
}

unset_routers () 
{
	local router_list

	if [ $# -lt 1 ]
	then
		return  1
	fi

	eval router_list="\$${1}_routers"

	for router in $router_list
	do
		route delete default $router >/dev/null 2>&1
	done
}

set_static_routes () 
{
	local static_routes

	if [ $# -lt 1 ]
	then
		return  1
	fi

	eval static_routes="\$${1}_static_routes"

	set static_routes

	while [ $# -ge 2 ]
	do
		$LOGGER "New Static Route: $1 -> $2"
		route add $1 $2
		shift; shift
	done
}

unset_static_routes () 
{
	local static_routes

	if [ $# -lt 1 ]
	then
		return  1
	fi

	eval static_routes="\$${1}_static_routes"

	set static_routes

	while [ $# -ge 2 ]
	do
		route delete $1 $2
		shift; shift
	done
}

#############################################################################
#
# utility functions grouping what needs to be done in logical units.
#
#############################################################################

set_all ()
{
	set_ip_address new
	set_routers new
	set_static_routes new

	if	[ "$new_ip_address" != "$alias_ip_address" ]
	then
		set_ip_alias
	fi
}

set_others ()
{
	update_hostname
	make_resolv_conf
}

clear_arp_table () 
{
	arp -d -a
}

unset_all ()
{
	if [ "$alias_ip_address" != "$old_ip_address" ]
	then
		unset_ip_alias
	fi

	if [ "$old_ip_address" != "" ] 
	then
		unset_ip_address old
		unset_routers old
		unset_static_routes old
		clear_arp_table
	fi
}

test_new_lease () 
{
	local rc

	set $new_routers

	if [ $# -ge 1 ]
	then
		set_ip_address new
   	if ping -q -c 1 $1
		then
			rc=0
		else
			rc=1
		fi
		unset_ip_address new
	else
		rc=1
	fi
	return  $rc
}

#############################################################################
# Main State functions.
#
# There is a state function for each state of the DHCP client
# These functions are OS specific and should be be tampered with.
#############################################################################

in_state_MEDIUM () 
{
  ifconfig $interface $medium
  ifconfig $interface inet -alias 0.0.0.0 $medium >/dev/null 2>&1
  sleep 1
  exit_status=0
}

in_state_PREINIT () 
{
	unset_ip_alias

	ifconfig $interface inet 0.0.0.0 netmask 0.0.0.0 \
			broadcast 255.255.255.255 up
	exit_status=0
}

in_state_ARPCHECK () 
{
  exit_status=0
}

in_state_ARPSEND () 
{
  exit_status=0
}

in_state_RENEW () 
{
	if [ "$old_ip_address" != "$new_ip_address" ]
	then
		unset_all
		set_all
	fi

	set_others
}

in_state_REBIND () {
	in_state_RENEW
}

in_state_BOUND () {
	unset_all
	set_all
	set_others
}

in_state_REBOOT () {
	in_state_BOUND
}

in_state_EXPIRE () 
{
	unset_all
	set_ip_alias
	exit_status=0
}

in_state_FAIL () {
	in_state_EXPIRE
}

in_state_TIMEOUT () 
{
	unset_all

	if test_new_lease
	then
		set_all
		set_others
	else
	 	$LOGGER "No good lease information in TIMEOUT state"	
		set_ip_alias
		exit_status=1
	fi
}

#############################################################################
# Main functions:
#
# dhclient_script_init() parses the optional "enter_hooks" script which can
#   override any of the state functions
#
# This function also parses the variables and notifies the detected changes.
#############################################################################
dhclient_script_init ()
{
	if [ -x /usr/bin/logger ]; then
		LOGGER="/usr/bin/logger -s -p user.notice -t dhclient"
	else
		LOGGER=echo
	fi

	# Invoke the local dhcp client enter hooks, if they exist.
	if [ -x /etc/dhclient-enter-hooks ]
	then
		exit_status=0
		. /etc/dhclient-enter-hooks
		# allow the local script to abort processing of this state
		# local script must set exit_status variable to nonzero.
		if [ $exit_status -ne 0 ]
		then
			exit $exit_status
		fi
	fi

	if [ "$new_network_number" != "" ]
	then
		$LOGGER "New Network Number: $new_network_number"
	fi

	if [ "$new_ip_address" != "" ]
	then
		$LOGGER "New IP Address: $new_ip_address"
	fi

	if [ "$new_broadcast_address" != "" ]
	then
		$LOGGER "New Broadcast Address: $new_broadcast_address"
	fi

	if [ "$new_subnet_mask" != "" ]
	then
		$LOGGER "New Subnet Mask for $interface: $new_subnet_mask"
	fi

	if [ "$alias_subnet_mask" != "" ]
	then
	fi
}

#############################################################################
# dhclient_main() does the appropriate work depending on the state of
# the dhcp client
#############################################################################
dhclient_script_main ()
{
#	set -x
	exit_status=0

	case $reason in
		MEDIUM|\
		PREINIT|\
		ARPCHECK|\
		ARPSEND|\
		RENEW|\
		REBIND|\
		BOUND|\
		REBOOT|\
		EXPIRE|\
		FAIL|\
		TIMEOUT)
			pre_state_${reason}_hook
			in_state_${reason}
			post_state_${reason}_hook
			;;
		*)
			$LOGGER "dhclient-script called with invalid reason $reason"
			exit_status=1
			;;
	esac

	exit_with_hooks $exit_status
}

#############################################################################
# Let's do the work...
#############################################################################

dhclient_script_init
dhclient_script_main
exit $exit_status

#############################################################################
# That's all folks
#############################################################################
