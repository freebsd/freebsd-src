#!/bin/sh

# $FreeBSD$

ARCH="`uname -m`"

# First check for the standard x86 PC class.
if [ "${ARCH}" = "i386" ]; then

sed	-e '/pty/d' \
	-e '/pass/d' \
	-e '/apm0/d' \
	-e '/ppp/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/RANDOMDEV/d' \
	-e '/atapist/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

echo "options  NETGRAPH"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"

# Otherwise maybe it's an alpha, and it has big binaries.
elif [ "${ARCH}" = "alpha" ]; then

sed	-e '/pty/d' \
	-e '/pass/d' \
	-e '/apm0/d' \
	-e '/ppp/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/RANDOMDEV/d' \
	-e '/atapist/d' \
	-e '/lpt/d' \
	-e '/ppi/d' \
	-e '/vpo/d' \
	-e '/uhci/d' \
	-e '/ohci/d' \
	-e '/usb/d' \
	-e '/ugen/d' \
	-e '/uhid/d' \
	-e '/ukbd/d' \
	-e '/ulpt/d' \
	-e '/umass/d' \
	-e '/ums/d' \
	-e '/aue/d' \
	-e '/cue/d' \
	-e '/kue/d' \
	-e '/maxusers/d' \
	-e 's/GENERIC/BOOTMFS/g'
fi

# reset maxusers to something lower
echo "maxusers	5"

echo "options  NFS_NOSERVER" 
echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
