#!/stand/sh
#
# netinst - configure the user's network.
#
# Written:  November 11th, 1994
# Copyright (C) 1994 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).
#
# $Id: netinst.sh,v 1.7 1994/11/18 19:09:33 jkh Exp $

if [ "$_NETINST_SH_LOADED_" = "yes" ]; then
	return 0
else
	_NETINST_SH_LOADED_=yes
fi

# Set some useful variables.
IFCONFIG=ifconfig
ROUTE=route
ROUTE_FLAGS="add default"

# Grab the miscellaneous functions.
. /stand/miscfuncs.sh

network_setup_ether()
{
	dialog $clear --title "Ethernet Interface Name" \
	--menu "Please select the type of ethernet interface you have:\n" \
	 -1 -1 7 \
	"ed0" "WD80x3, SMC, Novell NE[21]000 or 3C503 generic NIC at 0x280" \
	"ed1" "Same as above, but at address 0x300 and IRQ 5" \
	"ep0" "3COM 3C509 at address 0x300 and IRQ 10" \
	"de0" "DEC PCI ethernet adapter (or compatible)" \
	"ie0" "AT&T StarLan and EN100 family at 0x360 and IRQ 7" \
	"is0" "Isolan 4141-0 or Isolink 4110 at 0x280 and IRQ 7" \
	"ze0" "PCMCIA IBM or National card at 0x300 and IRQ 5" \
	  2> ${TMP}/menu.tmp.$$

	retval=$?
	interface=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval $retval; then return 1; fi
}

network_setup_slip()
{
	clear=""
	default_value=""
	if ! network_dialog "What is the IP number for the remote host?"; then return 1; fi
	remote_hostip=$answer
	interface=sl0

	default_value=$serial_interface
	if ! network_dialog "What is the name of the serial interface?"; then return 1; fi
	serial_interface=$answer

	default_value=$serial_speed
	if ! network_dialog "What speed is the serial interface?"; then return 1; fi
	serial_speed=$answer
	clear="--clear"

	if dialog $clear --title "Dial" --yesno "Do you need to dial the phone or otherwise talk to the modem?" -1 -1; then
		mkdir -p /var/log
		touch -f /var/log/aculog	> /dev/null 2>&1
		chmod 666 /var/log/aculog	> /dev/null 2>&1
		confirm "You may now dialog with your modem and set up the slip connection.\nBe sure to disable DTR sensitivity (usually with AT&D0) or the modem may\nhang up when you exit 'cu'.  Use ~. to exit cu and continue."
		dialog --clear
		# Grottyness to deal with a weird crunch bug.
		if [ ! -f /stand/cu ]; then ln /stand/tip /stand/cu; fi
		/stand/cu -l $serial_interface -s $serial_speed
		dialog --clear
	fi
}

network_setup_plip()
{
	default_value=""
	if ! network_dialog "What is the IP number for the remote host?"; then return 1; fi
	remote_hostip=$answer
	interface=lp0
}

network_setup()
{
	done=0
	while [ "$interface" = "" ]; do
		clear="--clear"
		dialog $clear --title "Set up network interface" \
		--menu "Please select the type of network connection you have:\n" \
		-1 -1 3 \
		"ether" "A supported ethernet card" \
		"SLIP" "A point-to-point SLIP (Serial Line IP) connection" \
		"PLIP" "A Parallel-Line IP setup (with standard laplink cable)" \
		2> ${TMP}/menu.tmp.$$

		retval=$?
		choice=`cat ${TMP}/menu.tmp.$$`
		rm -f ${TMP}/menu.tmp.$$
		if ! handle_rval $retval; then return 1; fi
		case $choice in
		ether)
			if ! network_setup_ether; then continue; fi
		;;

		SLIP)
			if ! network_setup_slip; then continue; fi
		;;

		PLIP)
			if ! network_setup_plip; then continue; fi
		;;
		esac	
		if [ "$interface" = "" ]; then	continue; fi

		clear=""
		default_value=""
		if ! network_dialog "What is the fully qualified name of this host?"; then clear="--clear"; return 1; fi
		hostname=$answer
		echo $hostname > /etc/myname
		hostname $hostname

		default_value=`echo $hostname | sed -e 's/[^.]*\.//'`
		if network_dialog "What is the domain name of this host (Internet, not YP/NIS)?"; then
			domain=$answer
		fi

		default_value=""
		if ! network_dialog "What is the IP address of this host?"; then clear="--clear"; return 1; fi
		ipaddr=$answer

        	echo "$ipaddr    $hostname `echo $hostname | sed -e 's/\.$domain//'`" >> /etc/hosts

		default_value="$netmask"
		if network_dialog "Please specify the netmask"; then
			if [ "$answer" != "" ]; then
				netmask=$answer
			fi
		fi

		default_value=""
		if network_dialog "Any extra flags to ifconfig?" ; then
			ifconfig_flags=$answer
		fi
		echo "Progress <$IFCONFIG $interface $ipaddr $remote_hostip netmask $netmask $ifconfig_flags>" >/dev/ttyv1
		if ! $IFCONFIG $interface $ipaddr $remote_hostip netmask $netmask $ifconfig_flags > /dev/ttyv1 2>&1 ; then
			error "Unable to configure interface $interface"
			ipaddr=""; interface=""
			continue
		fi
		if [ "$interface" = "sl0" ]; then
			slattach -a -s $serial_speed $serial_interface
		fi
		echo "$ipaddr $remote_hostip netmask $netmask $ifconfig_flags" > /etc/hostname.$interface
		default_value=""
		if network_dialog "If you have a default gateway, enter its IP address"; then
			if [ "$answer" != "" ]; then
				gateway=$answer
				echo "Progress <$ROUTE $ROUTE_FLAGS $gateway>" > /dev/ttyv1 2>&1
				$ROUTE $ROUTE_FLAGS $gateway > /dev/ttyv1 2>&1
				echo $gateway > /etc/defaultrouter
			fi
		fi

		default_value=""
		if network_dialog "If you have a name server, enter its IP address"; then
			if [ "$answer" != "" ]; then
				nameserver=$answer
				echo "domain $domain" > /etc/resolv.conf
				echo "nameserver $nameserver" >> /etc/resolv.conf
			fi
		fi
	done
	return 0
}
