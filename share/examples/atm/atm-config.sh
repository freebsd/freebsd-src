#! /bin/sh
#
#
# ===================================
# HARP  |  Host ATM Research Platform
# ===================================
#
#
# This Host ATM Research Platform ("HARP") file (the "Software") is
# made available by Network Computing Services, Inc. ("NetworkCS")
# "AS IS".  NetworkCS does not provide maintenance, improvements or
# support of any kind.
#
# NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
# INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
# SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
# In no event shall NetworkCS be responsible for any damages, including
# but not limited to consequential damages, arising from or relating to
# any use of the Software or related support.
#
# Copyright 1994-1998 Network Computing Services, Inc.
#
# Copies of this Software may be made, however, the above copyright
# notice must be reproduced on all copies.
#
#	@(#) $FreeBSD: src/share/examples/atm/atm-config.sh,v 1.2 1999/08/28 00:19:07 peter Exp $
#
#

#
# Sample script to load and configure ATM software
#

#
# Download FORE microcode into adapter(s)
#
# This step is only required if you are using FORE ATM adapters.
# This assumes that the FORE microcode file pca200e.bin is in /etc.
# See the file fore-microcode.txt for further details.
#
/sbin/fore_dnld -d /etc

#
# Define network interfaces
#
/sbin/atm set netif hfa0 <netif_prefix> 1

#
# Configure physical interfaces
#
/sbin/atm attach hfa0 uni31

#
# Start ILMI daemon (optional)
#
/sbin/ilmid

#
# Set ATM address prefix
#
# Only need to set prefix if using UNI and not using ILMI daemon
#
#/sbin/atm set prefix hfa0 <nsap_prefix>

#
# Configure network interfaces
#
/sbin/ifconfig <netif> <ip_addr> netmask + up
/sbin/atm set arpserver <netif> <atm_address>

#
# Configure PVCs (optional)
#
#/sbin/atm add pvc hfa0 <vpi> <vci> aal5 null ip <netif> <ip_addr>

#
# Start SCSP daemons (optional)
#
# This step is only required if your host is configured as an ATMARP server
# and you wish to synchronize its cache with the cache(s) of some other
# server(s).  Scspd will look for its configuration file at /etc/scspd.conf.
#
#/usr/sbin/scspd
#/usr/sbin/atmarpd <netif> ...

exit 0

