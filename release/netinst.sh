#!/stand/sh
#
# netinst.sh - configure the user's network.
#
# Written:  November 11th, 1994
# Copyright (C) 1994 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).
#
# $Id: netinst.sh,v 1.13 1994/12/01 20:00:32 jkh Exp $

if [ "${_NETINST_SH_LOADED_}" = "yes" ]; then
	return 0
else
	_NETINST_SH_LOADED_=yes
fi

# Grab the miscellaneous functions.
. /stand/miscfuncs.sh

network_set_defaults()
{
	HOSTNAME=""
	DOMAIN=""
	NETMASK="0xffffff00"
	IPADDR="127.0.0.1"
	IFCONFIG_FLAGS=""
	REMOTE_HOSTIP=""
	REMOTE_IPADDR=""
	INTERFACE=""
	SERIAL_INTERFACE="/dev/tty00"
	SERIAL_SPEED="38400"
}

network_basic_setup()
{
	HOSTNAME=""
	while [ "${HOSTNAME}" = "" ]; do
		DEFAULT_VALUE=""
		if ! network_dialog "What is the fully qualified name of this host?"; then return 1; fi
		if [ "${ANSWER}" = "" ]; then
			error "You must select a host name!"
			continue
		else
			HOSTNAME=${ANSWER}
		fi
	done
	echo ${HOSTNAME} > ${ETC}/myname
	${HOSTNAME_CMD} ${HOSTNAME}

	DEFAULT_VALUE=`echo ${HOSTNAME} | sed -e 's/[^.]*\.//' | grep \.`
	if network_dialog "What is the domain name of this host (Internet, not YP/NIS)?"; then
		DOMAIN=${ANSWER}
	fi

	DEFAULT_VALUE=${IPADDR}
	if ! network_dialog "What is the IP address of this host?"; then return 1; fi
	IPADDR=${ANSWER}
       	echo "${IPADDR} ${HOSTNAME} `echo ${HOSTNAME} | sed -e 's/\.${DOMAIN}//'`" >> ${ETC}/hosts
}

network_setup_ether()
{
	dialog  --title "Ethernet Interface Name" --menu \
	"Please select the type of ethernet interface you have:\n" -1 -1 8 \
	"ed0" "WD80x3, SMC, Novell NE[21]000 or 3C503 generic NIC at 0x280" \
	"ed1" "Same as above, but at address 0x300 and IRQ 5" \
	"ep0" "3COM 3C509 at address 0x300 and IRQ 10" \
	"de0" "DEC PCI ethernet adapter (or compatible)" \
	"ie0" "AT&T StarLan and EN100 family at 0x360 and IRQ 7" \
	"is0" "Isolan 4141-0 or Isolink 4110 at 0x280 and IRQ 7" \
	"le0" "DEC Etherworks ethernet adapter" \
	"ze0" "PCMCIA IBM or National card at 0x300 and IRQ 5" \
	  2> ${TMP}/menu.tmp.$$

	RETVAL=$?
	INTERFACE=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval ${RETVAL}; then return 1; fi
}

network_setup_remote()
{
	DEFAULT_VALUE="${REMOTE_IPADDR}"
	if ! network_dialog "What is the IP number for the remote host?"; then
		return 1
	fi
	REMOTE_IPADDR=${ANSWER}
}

network_setup_serial()
{
	network_setup_remote
	INTERFACE=$1

	DEFAULT_VALUE=${SERIAL_INTERFACE}
	if ! network_dialog "What serial port do you wish to use?"; then
		return 1
	fi
	SERIAL_INTERFACE=${ANSWER}

	DEFAULT_VALUE=${SERIAL_SPEED}
	if ! network_dialog "What speed is the serial connection?"; then
		return 1
	fi
	SERIAL_SPEED=${ANSWER}

	if dialog --title "Dial" --yesno \
	  "Do you need to dial the phone or otherwise talk to the modem?" \
	  -1 -1; then
		mkdir -p /var/log
		touch -f /var/log/aculog	> /dev/null 2>&1
		chmod 666 /var/log/aculog	> /dev/null 2>&1
		confirm \
"You may now dialog with your modem and set up the connection.
Be sure to disable DTR sensitivity (usually with AT&D0) or the
modem may hang up when you exit 'cu'.  Use ~. to exit cu and
continue."
		dialog --clear
		# Grottyness to deal with a weird crunch bug.
		if [ ! -f /stand/cu ]; then ln /stand/tip /stand/cu; fi
		/stand/cu -l ${SERIAL_INTERFACE} -s ${SERIAL_SPEED}
		dialog --clear
	fi
}

network_setup_plip()
{
	network_setup_remote
	INTERFACE=lp0
}

network_setup()
{
	DONE=0
	while [ "${INTERFACE}" = "" ]; do
		dialog --title "Set up network interface" --menu \
		  "Please select the type of network connection you have:\n" \
		  -1 -1 4 \
		"Ether" "A supported ethernet card" \
		"SLIP" "A point-to-point SLIP (Serial Line IP) connection" \
		"PPP" "A Point-To-Point-Protocol connection" \
		"PLIP" "A Parallel-Line IP setup (with standard laplink cable)" \
		2> ${TMP}/menu.tmp.$$

		RETVAL=$?
		CHOICE=`cat ${TMP}/menu.tmp.$$`
		rm -f ${TMP}/menu.tmp.$$
		if ! handle_rval ${RETVAL}; then return 1; fi
		case ${CHOICE} in
		Ether)	if ! network_setup_ether; then continue; fi ;;
		SLIP)	if ! network_setup_serial sl0; then continue; fi ;;

		PPP)	if ! network_setup_serial ppp0; then continue; fi ;;

		PLIP)	if ! network_setup_plip; then continue; fi ;;
		esac
		if [ "${INTERFACE}" = "" ]; then continue; fi

		network_basic_setup

		DEFAULT_VALUE="${NETMASK}"
		if network_dialog "Please specify the netmask"; then
			if [ "${ANSWER}" != "" ]; then
				NETMASK=${ANSWER}
			fi
		fi

		DEFAULT_VALUE=""
		if network_dialog "Set extra flags to ${IFCONFIG}?"; then
			IFCONFIG_FLAGS=${ANSWER}
		fi
		echo "Progress <${IFCONFIG_CMD} ${INTERFACE} ${IPADDR} ${REMOTE_IPADDR} netmask ${NETMASK} ${IFCONFIG_FLAGS}>" >/dev/ttyv1
		if ! ${IFCONFIG_CMD} ${INTERFACE} ${IPADDR} ${REMOTE_IPADDR} netmask ${NETMASK} ${IFCONFIG_FLAGS} > /dev/ttyv1 2>&1 ; then
			error "Unable to configure interface ${INTERFACE}"
			IPADDR=""
			INTERFACE=""
			continue
		fi
		if [ "${INTERFACE}" = "sl0" ]; then
			DEFAULT_VALUE=${SLATTACH_FLAGS}
			if network_dialog "Set extra flags to ${SLATTACH_CMD}?"; then
				SLATTACH_FLAGS=${ANSWER}
			fi
			${SLATTACH_CMD} ${SLATTACH_FLAGS} ${SERIAL_SPEED} ${SERIAL_INTERFACE}
			progress ${SLATTACH_CMD} ${SLATTACH_FLAGS} ${SERIAL_SPEED} ${SERIAL_INTERFACE}
		fi
		if [ "${INTERFACE}" = "ppp0" ]; then
			DEFAULT_VALUE=${PPPD_FLAGS}
			if network_dialog "Set extra flags to ${PPPD}?"; then
				PPPD_FLAGS=${ANSWER}
			fi
			${PPPD_CMD} ${PPPD_FLAGS} ${SERIAL_INTERFACE} ${SERIAL_SPEED} ${IPADDR}:${REMOTE_IPADDR}
			progress ${PPPD_CMD} ${PPPD_FLAGS} ${SERIAL_INTERFACE} ${SERIAL_SPEED} ${IPADDR}:${REMOTE_IPADDR}
		fi
		echo "${IPADDR} ${REMOTE_IPADDR} netmask ${NETMASK} ${IFCONFIG_FLAGS}" > ${ETC}/hostname.${INTERFACE}
		DEFAULT_VALUE=""
		if network_dialog "If you have a default gateway, enter its IP address"; then
			if [ "${ANSWER}" != "" ]; then
				GATEWAY=${ANSWER}
				${ROUTE_CMD} ${ROUTE_FLAGS} ${GATEWAY} > /dev/ttyv1 2>&1
				progress ${ROUTE_CMD} ${ROUTE_FLAGS} ${GATEWAY}
				echo ${GATEWAY} > ${ETC}/defaultrouter
			fi
		fi

		DEFAULT_VALUE=""
		if network_dialog "If you have a name server, enter its IP address"; then
			if [ "${ANSWER}" != "" ]; then
				NAMESERVER=${ANSWER}
				echo "domain ${DOMAIN}" > ${ETC}/resolv.conf
				echo "nameserver ${NAMESERVER}" >> ${ETC}/resolv.conf
			fi
		fi
	done
	return 0
}
