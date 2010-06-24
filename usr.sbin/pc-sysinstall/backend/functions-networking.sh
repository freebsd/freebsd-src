#!/bin/sh
#-
# Copyright (c) 2010 iX Systems, Inc.  All rights reserved.
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

# Functions which perform our networking setup

# Function which creates a kde4 .desktop file for the PC-BSD net tray
create_desktop_nettray()
{
  NIC="${1}"
  echo "#!/usr/bin/env xdg-open
[Desktop Entry]
Exec=/usr/local/kde4/bin/pc-nettray ${NIC}
Icon=network
StartupNotify=false
Type=Application" > ${FSMNT}/usr/share/skel/.kde4/Autostart/tray-${NIC}.desktop
  chmod 744 ${FSMNT}/usr/share/skel/.kde4/Autostart/tray-${NIC}.desktop

};

# Function which checks is a nic is wifi or not
check_is_wifi()
{
  NIC="$1"
  ifconfig ${NIC} | grep "802.11" >/dev/null 2>/dev/null
  if [ "$?" = "0" ]
  then
    return 0
  else 
    return 1
  fi
};

# Function to get the first available wired nic, used for lagg0 setup
get_first_wired_nic()
{
  rm ${TMPDIR}/.niclist >/dev/null 2>/dev/null
  # start by getting a list of nics on this system
  ${QUERYDIR}/detect-nics.sh > ${TMPDIR}/.niclist
  if [ -e "${TMPDIR}/.niclist" ]
  then
    while read line
    do
      NIC="`echo $line | cut -d ':' -f 1`"
      check_is_wifi ${NIC}
      if [ "$?" != "0" ]
      then
         VAL="${NIC}" ; export VAL
         return
      fi
    done < ${TMPDIR}/.niclist
  fi

  VAL="" ; export VAL
  return
};

# Function which simply enables plain dhcp on all detected nics, not fancy lagg interface
enable_plain_dhcp_all()
{
  rm ${TMPDIR}/.niclist >/dev/null 2>/dev/null
  # start by getting a list of nics on this system
  ${QUERYDIR}/detect-nics.sh > ${TMPDIR}/.niclist
  if [ -e "${TMPDIR}/.niclist" ]
  then
    echo "# Auto-Enabled NICs from pc-sysinstall" >>${FSMNT}/etc/rc.conf
    WLANCOUNT="0"
    while read line
    do
      NIC="`echo $line | cut -d ':' -f 1`"
      DESC="`echo $line | cut -d ':' -f 2`"
      echo_log "Setting $NIC to DHCP on the system."
      check_is_wifi ${NIC}
      if [ "$?" = "0" ]
      then
        # We have a wifi device, setup a wlan* entry for it
        WLAN="wlan${WLANCOUNT}"
        echo "wlans_${NIC}=\"${WLAN}\"" >>${FSMNT}/etc/rc.conf
        echo "ifconfig_${WLAN}=\"DHCP\"" >>${FSMNT}/etc/rc.conf
        CNIC="${WLAN}"
        WLANCOUNT="`expr ${WLANCOUNT} + 1`"
      else
        echo "ifconfig_${NIC}=\"DHCP\"" >>${FSMNT}/etc/rc.conf
        CNIC="${NIC}"
      fi
 
    done < ${TMPDIR}/.niclist 
  fi
};

# Function which enables fancy lagg dhcp on specified wifi 
enable_lagg_dhcp()
{
  WIFINIC="$1"
  
  # Get the first wired nic
  get_first_wired_nic
  WIRENIC=$VAL
  LAGGPORT="laggport ${WIFINIC}"

  echo "# Auto-Enabled NICs from pc-sysinstall" >>${FSMNT}/etc/rc.conf
  if [ ! -z "$WIRENIC" ]
  then
    echo "ifconfig_${WIRENIC}=\"up\"" >> ${FSMNT}/etc/rc.conf
    echo "ifconfig_${WIFINIC}=\"\`ifconfig ${WIRENIC} ether\`\"" >> ${FSMNT}/etc/rc.conf
    echo "ifconfig_${WIFINIC}=\"ether \${ifconfig_${WIFINIC}##*ether }\"" >> ${FSMNT}/etc/rc.conf
    LAGGPORT="laggport ${WIRENIC} ${LAGGPORT}"
  fi
  
  echo "wlans_${WIFINIC}=\"wlan0\"" >> ${FSMNT}/etc/rc.conf
  echo "cloned_interfaces=\"lagg0\"" >> ${FSMNT}/etc/rc.conf
  echo "ifconfig_lagg0=\"laggproto failover ${LAGGPORT} DHCP\"" >> ${FSMNT}/etc/rc.conf

};

# Function which detects available nics, and runs them to DHCP on the
save_auto_dhcp()
{
  rm ${TMPDIR}/.niclist >/dev/null 2>/dev/null
  # start by getting a list of nics on this system
  ${QUERYDIR}/detect-nics.sh > ${TMPDIR}/.niclist
  if [ -e "${TMPDIR}/.niclist" ]
  then
    while read line
    do
      NIC="`echo $line | cut -d ':' -f 1`"
      DESC="`echo $line | cut -d ':' -f 2`"
      check_is_wifi "${NIC}"
      if [ "$?" = "0" ]
      then
        # We have a wifi device, lets do fancy lagg interface
        enable_lagg_dhcp "${NIC}"
        return
      fi
 
    done < ${TMPDIR}/.niclist 
  fi

  # Got here, looks like no wifi, so lets simply enable plain-ole-dhcp
  enable_plain_dhcp_all

};


# Function which saves a manual nic setup to the installed system
save_manual_nic()
{
  # Get the target nic
  NIC="$1"

  get_value_from_cfg netSaveIP
  NETIP="${VAL}"
 
  if [ "$NETIP" = "DHCP" ]
  then
    echo_log "Setting $NIC to DHCP on the system."
    echo "ifconfig_${NIC}=\"DHCP\"" >>${FSMNT}/etc/rc.conf
    return 0
  fi

  # If we get here, we have a manual setup, lets do so now

  # Set the manual IP
  IFARGS="inet ${NETIP}"

  # Check if we have a netmask to set
  get_value_from_cfg netSaveMask
  NETMASK="${VAL}"
  if [ ! -z "${NETMASK}" ]
  then
    IFARGS="${IFARGS} netmask ${NETMASK}"
  fi


  echo "# Auto-Enabled NICs from pc-sysinstall" >>${FSMNT}/etc/rc.conf
  echo "ifconfig_${NIC}=\"${IFARGS}\"" >>${FSMNT}/etc/rc.conf

  # Check if we have a default router to set
  get_value_from_cfg netSaveDefaultRouter
  NETROUTE="${VAL}"
  if [ ! -z "${NETROUTE}" ]
  then
    echo "defaultrouter=\"${NETROUTE}\"" >>${FSMNT}/etc/rc.conf
  fi

  # Check if we have a nameserver to enable
  get_value_from_cfg netSaveNameServer
  NAMESERVER="${VAL}"
  if [ ! -z "${NAMESERVER}" ]
  then
    echo "nameserver ${NAMESERVER}" >${FSMNT}/etc/resolv.conf
  fi
 
};

# Function which determines if a nic is active / up
is_nic_active()
{
  ifconfig ${1} | grep "status: active" >/dev/null 2>/dev/null
  if [ "$?" = "0" ] ; then
    return 0
  else
    return 1
  fi
};


# Function which detects available nics, and runs DHCP on them until
# a success is found
enable_auto_dhcp()
{
  # start by getting a list of nics on this system
  ${QUERYDIR}/detect-nics.sh > ${TMPDIR}/.niclist
  while read line
  do
    NIC="`echo $line | cut -d ':' -f 1`"
    DESC="`echo $line | cut -d ':' -f 2`"

    is_nic_active "${NIC}"
    if [ "$?" = "0" ] ; then
    	echo_log "Trying DHCP on $NIC $DESC"
    	dhclient ${NIC} >/dev/null 2>/dev/null
    	if [ "$?" = "0" ] ; then
   	   # Got a valid DHCP IP, we can return now
	   WRKNIC="$NIC" ; export WRKNIC
   	   return 0
	fi
    fi
  done < ${TMPDIR}/.niclist 

};

# Get the mac address of a target NIC
get_nic_mac() {
	FOUNDMAC="`ifconfig ${1} | grep 'ether' | tr -d '\t' | cut -d ' ' -f 2`"
	export FOUNDMAC
}

# Function which performs the manual setup of a target nic in the cfg
enable_manual_nic()
{
  # Get the target nic
  NIC="$1"

  # Check that this NIC exists
  rc_halt "ifconfig ${NIC}"

  get_value_from_cfg netIP
  NETIP="${VAL}"
  
  if [ "$NETIP" = "DHCP" ]
  then
    echo_log "Enabling DHCP on $NIC"
    rc_halt "dhclient ${NIC}"
    return 0
  fi

  # If we get here, we have a manual setup, lets do so now

  # Set the manual IP
  rc_halt "ifconfig ${NIC} ${NETIP}"

  # Check if we have a netmask to set
  get_value_from_cfg netMask
  NETMASK="${VAL}"
  if [ ! -z "${NETMASK}" ]
  then
    rc_halt "ifconfig ${NIC} netmask ${NETMASK}"
  fi

  # Check if we have a default router to set
  get_value_from_cfg netDefaultRouter
  NETROUTE="${VAL}"
  if [ ! -z "${NETROUTE}" ]
  then
    rc_halt "route add default ${NETROUTE}"
  fi

  # Check if we have a nameserver to enable
  get_value_from_cfg netNameServer
  NAMESERVER="${VAL}"
  if [ ! -z "${NAMESERVER}" ]
  then
    echo "nameserver ${NAMESERVER}" >/etc/resolv.conf
  fi
  
  
};


# Function which parses the cfg and enables networking per specified
start_networking()
{
  # Check if we have any networking requested
  get_value_from_cfg netDev
  if [ -z "${VAL}" ]
  then
    return 0
  fi

  NETDEV="${VAL}"
  if [ "$NETDEV" = "AUTO-DHCP" ]
  then
    enable_auto_dhcp
  else
    enable_manual_nic ${NETDEV}
  fi

};


# Function which checks the cfg and enables the specified networking on
# the installed system
save_networking_install()
{

  # Check if we have any networking requested to save
  get_value_from_cfg netSaveDev
  if [ -z "${VAL}" ]
  then
    return 0
  fi

  NETDEV="${VAL}"
  if [ "$NETDEV" = "AUTO-DHCP" ]
  then
    save_auto_dhcp
  else
    save_manual_nic ${NETDEV}
  fi

};

