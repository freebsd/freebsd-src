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

#
# ATM networking startup script
#
# Initial interface configuration.
# N.B. /usr is not mounted.
#
atm_pass1() {
	# Locate all probed ATM adapters
	atmdev=`atm sh stat int | while read dev junk; do
		case ${dev} in
		hea[0-9] | hea[0-9][0-9])
			echo "${dev} "
			;;
		hfa[0-9] | hfa[0-9][0-9])
			echo "${dev} "
			;;
		*)
			continue
			;;
		esac
	done`

	if [ -z "${atmdev}" ]; then
		echo 'No ATM adapters found'
		return 0
	fi

	# Load microcode into FORE adapters (if needed)
	if [ `expr "${atmdev}" : '.*hfa.*'` -ne 0 ]; then
		fore_dnld
	fi

	# Configure physical interfaces
	ilmid=0
	for phy in ${atmdev}; do
		echo -n "Configuring ATM device ${phy}:"

		# Define network interfaces
		eval netif_args=\$atm_netif_${phy}
		if [ -n "${netif_args}" ]; then
			atm set netif ${phy} ${netif_args} || continue
		else
			echo ' missing network interface definition'
			continue
		fi

		# Override physical MAC address
		eval macaddr_args=\$atm_macaddr_${phy}
		if [ -n "${macaddr_args}" ]; then
			case ${macaddr_args} in
			[Nn][Oo] | '')
				;;
			*)
				atm set mac ${phy} ${macaddr_args} || continue
				;;
			esac
		fi

		# Configure signalling manager
		eval sigmgr_args=\$atm_sigmgr_${phy}
		if [ -n "${sigmgr_args}" ]; then
			atm attach ${phy} ${sigmgr_args} || continue
		else
			echo ' missing signalling manager definition'
			continue
		fi

		# Configure UNI NSAP prefix
		eval prefix_args=\$atm_prefix_${phy}
		if [ `expr "${sigmgr_args}" : '[uU][nN][iI].*'` -ne 0 ]; then
			if [ -z "${prefix_args}" ]; then
				echo ' missing NSAP prefix for UNI interface'
				continue
			fi

			case ${prefix_args} in
			ILMI)
				ilmid=1
				;;
			*)
				atm set prefix ${phy} ${prefix_args} || continue
				;;
			esac
		fi

		atm_phy="${atm_phy} ${phy}"
		echo '.'
	done

	echo -n 'Starting initial ATM daemons:'
	# Start ILMI daemon (if needed)
	case ${ilmid} in
	1)
		echo -n ' ilmid'
		ilmid
		;;
	esac

	echo '.'
	atm_pass1_done=YES
}

#
# Finish up configuration.
# N.B. /usr is not mounted.
#
atm_pass2() {
	echo -n 'Configuring ATM network interfaces:'

	atm_scspd=0
	atm_atmarpd=""

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
			esac
		done
	done
	echo '.'

	# Define any PVCs.
	if [ -n "${atm_pvcs}" ]; then
		for i in ${atm_pvcs}; do
			eval pvc_args=\$atm_pvc_${i}
			atm add pvc ${pvc_args}
		done
	fi

	# Define any permanent ARP entries.
	if [ -n "${atm_arps}" ]; then
		for i in ${atm_arps}; do
			eval arp_args=\$atm_arp_${i}
			atm add arp ${arp_args}
		done
	fi
	atm_pass2_done=YES
}

#
# Start any necessary daemons.
#
atm_pass3() {
	# Start SCSP daemon (if needed)
	case ${atm_scspd} in
	1)
		echo -n ' scspd'
		scspd
		;;
	esac

	# Start ATMARP daemon (if needed)
	if [ -n "${atm_atmarpd}" ]; then
		echo -n ' atmarpd'
		atmarpd ${atm_atmarpd}
	fi

	atm_pass3_done=YES
}
