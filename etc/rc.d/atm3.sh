#!/bin/sh
#

# ATM networking startup script
#
#	$Id$

#
# Initial interface configuration.
# N.B. /usr is not mounted.
#
atm_pass1() {
    # Locate all probed ATM adapters
    atmdev=`atm sh stat int | while read dev junk; do
	case ${dev} in
	hea[0-9]|hea[0-9][0-9])
		echo "${dev} "
		;;
	hfa[0-9]|hfa[0-9][0-9])
		echo "${dev} "
		;;
	*)
		continue
		;;
	esac
    done`

    if [ -z "${atmdev}" ]; then
	echo "No ATM adapters found."
	return 0
    fi

    # Load microcode into FORE adapters (if needed)
    if [ `expr "${atmdev}" : '.*hfa.*'` -ne 0 ]; then
	fore_dnld -d /etc
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
		echo "missing network interface definition"
		continue
	fi

	# Override physical MAC address
	eval macaddr_args=\$atm_macaddr_${phy}
	if [ -n "${macaddr_args}" -a "${macaddr_args}" != "NO" ]; then
		atm set mac ${phy} ${macaddr_args} || continue
	fi

	# Configure signalling manager
	eval sigmgr_args=\$atm_sigmgr_${phy}
	if [ -n "${sigmgr_args}" ]; then
		atm attach ${phy} ${sigmgr_args} || continue
	else
		echo "missing signalling manager definition"
		continue
	fi

	# Configure UNI NSAP prefix
	eval prefix_args=\$atm_prefix_${phy}
	if [ `expr "${sigmgr_args}" : '[uU][nN][iI].*'` -ne 0 ]; then
		if [ -z "${prefix_args}" ]; then
			echo "missing NSAP prefix for UNI interface"
			continue
		fi
		if [ "${prefix_args}" = "ILMI" ]; then
			ilmid=1
		else
			atm set prefix ${phy} ${prefix_args} || continue
		fi
	fi

	atm_phy="${atm_phy} ${phy}"
	echo "."
    done

    echo -n "Starting initial ATM daemons:"
    # Start ILMI daemon (if needed)
    if [ ${ilmid} -eq 1 ]; then
	echo -n " ilmid"
	ilmid
    fi

    echo "."
    atm_pass1_done=YES
}

#
# Finish up configuration.
# N.B. /usr is not mounted.
#
atm_pass2() {
    echo -n "Configuring ATM network interfaces:"

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
		netindx=`expr ${netindx} + 1`
		echo -n " ${net}"

		# Configure atmarp server
		eval atmarp_args=\$atm_arpserver_${net}
		if [ -n "${atmarp_args}" ]; then
			atm set arpserver ${net} ${atmarp_args} || continue
		fi
		eval scsparp_args=\$atm_scsparp_${net}
		if [ "X${scsparp_args}" = X"YES" ]; then
			if [ "${atmarp_args}" != "local" ]; then
				echo "local arpserver required for SCSP"
				continue
			fi
			atm_atmarpd="${atm_atmarpd} ${net}"
			atm_scspd=1
		fi
	done
    done
    echo "."

    # Define any PVCs.
    if [ "X${atm_pvcs}" != "X" ]; then
	for i in ${atm_pvcs}; do
		eval pvc_args=\$atm_pvc_${i}
		atm add pvc ${pvc_args}
	done
    fi

    # Define any permanent ARP entries.
    if [ "X${atm_arps}" != "X" ]; then
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
    if [ ${atm_scspd} -eq 1 ]; then
	echo -n " scspd"
	scspd
    fi

    # Start ATMARP daemon (if needed)
    if [ -n "${atm_atmarpd}" ]; then
	echo -n " atmarpd"
	atmarpd ${atm_atmarpd}
    fi

    atm_pass3_done=YES
}
