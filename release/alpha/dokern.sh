#!/bin/sh

ARCH="`uname -m`"

# First check for the standard x86 PC class.
if [ "${ARCH}" = "i386" ]; then

sed	-e '/pty/d' \
	-e '/pass/d' \
	-e '/apm0/d' \
	-e '/ppp/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/IPv6/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

echo "options  NETGRAPH"
echo "options  NETGRAPH_ETHER"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"

# Otherwise maybe it's an alpha, and it has big binaries.
elif [ "${ARCH}" = "alpha" ]; then

sed	-e '/pty/d' \
	-e '/pass/d' \
	-e '/apm0/d' \
	-e '/ppp/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/IPv6/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/SOFTUPDATES/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'
fi

# reset maxusers to something lower
echo "maxusers	5"

echo "options  NFS_NOSERVER" 
echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
