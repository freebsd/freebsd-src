#!/bin/sh
#
# Copyright (c) 2000  The FreeBSD Project
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
# $FreeBSD$
#

# PROVIDE: atm2
# REQUIRE: atm1 netif
# BEFORE:  routing
# KEYWORD: FreeBSD

#
# Additional ATM interface configuration
#

. /etc/rc.subr

atm2_start()
{
	# Configure network interfaces
	for phy in ${atm_phy}; do
		eval netif_args=\$atm_netif_${phy}
		set -- ${netif_args}
		netname=$1
		netcnt=$2
		netindx=0
		while [ ${netindx} -lt ${netcnt} ]; do
			net="${netname}${netindx}"
			netindx=$((${netindx} + 1))
			echo -n " ${net}"

			# Configure atmarp server
			eval atmarp_args=\$atm_arpserver_${net}
			if [ -n "${atmarp_args}" ]; then
				atm set arpserver ${net} ${atmarp_args} ||
				    continue
			fi
			eval scsparp_args=\$atm_scsparp_${net}

			case ${scsparp_args} in
			[Yy][Ee][Ss])
				case ${atmarp_args} in
				local)
					;;
				*)
					echo ' local arpserver required for SCSP'
					continue
					;;
				esac

				atm_atmarpd="${atm_atmarpd} ${net}"
				atm_scspd=1
				;;
			esac
		done
	done
	echo '.'

	# Define any permanent ARP entries.
	if [ -n "${atm_arps}" ]; then
		for i in ${atm_arps}; do
			eval arp_args=\$atm_arp_${i}
			atm add arp ${arp_args}
		done
	fi

	# XXX - required by atm3.sh. I don't like having one script depend
	#       on variables in another script (especially in a dynamic
	#       ordered system like this), but it's necessary for the moment.
	#
	export atm_atmarpd
	export atm_scspd
}

load_rc_config "XXX"

case ${atm_enable} in
[Yy][Ee][Ss])
	atm2_start
	;;
esac
