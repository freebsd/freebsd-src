#!/bin/sh
#
# $FreeBSD$
#

if [ $# -lt 1 ]; then
	FDSIZE=NORMAL
else
	FDSIZE=$1
fi

ARCH="`uname -m`"

# First check for the standard x86 PC class.
if [ "${ARCH}" = "i386" ]; then

if [ "${FDSIZE}" = "SMALL" ]; then

sed	-e '/	pci$/d' \
	-e '/	adv/d' \
	-e '/	ahc/d' \
	-e '/	amd/d' \
	-e '/	isp/d' \
	-e '/	ncr/d' \
	-e '/	sym/d' \
	-e '/	de/d' \
	-e '/	fxp/d' \
	-e '/	tx/d' \
	-e '/	vx/d' \
	-e '/	wx/d' \
	-e '/	dc/d' \
	-e '/	pcn/d' \
	-e '/	rl/d' \
	-e '/	sf/d' \
	-e '/	sis/d' \
	-e '/	ste/d' \
	-e '/	tl/d' \
	-e '/	vr/d' \
	-e '/	wb/d' \
	-e '/	xl/d' \
	-e '/pty/d' \
	-e '/pass/d' \
	-e '/	apm/d' \
	-e '/pmtimer/d' \
	-e '/ppp/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/gzip/d' \
	-e '/random/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/pcm/d' \
	-e '/atapist/d' \
	-e '/ugen/d' \
	-e '/uhid/d' \
	-e '/ulpt/d' \
	-e '/urio/d' \
	-e '/uscanner/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

else

sed	-e '/pty/d' \
	-e '/pass/d' \
	-e '/	apm/d' \
	-e '/pmtimer/d' \
	-e '/ppp/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/gzip/d' \
	-e '/random/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/	ncr/d' \
	-e '/pcm/d' \
	-e '/atapist/d' \
	-e '/wds/d' \
	-e '/lpt/d' \
	-e '/ppi/d' \
	-e '/ugen/d' \
	-e '/uhid/d' \
	-e '/ulpt/d' \
	-e '/urio/d' \
	-e '/uscanner/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

fi

echo "options  NETGRAPH"
echo "options  NETGRAPH_ETHER"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"

# Otherwise maybe it's an alpha, and it has big binaries.
elif [ "${ARCH}" = "alpha" ]; then

sed	\
	-e '/DEC_3000_300/d' \
	-e '/DEC_3000_500/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/MSDOSFS/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/P1003_1B/d' \
	-e '/_KPOSIX_PRIORITY_SCHEDULING/d' \
	-e '/atapist/d' \
	-e '/ncr/d' \
	-e '/pass/d' \
	-e '/splash/d' \
	-e '/	apm/d' \
	-e '/pmtimer/d' \
	-e '/pcm/d' \
	-e '/lpt/d' \
	-e '/ppi/d' \
	-e '/vpo/d' \
	-e '/	le	/d' \
	-e '/random/d' \
	-e '/gzip/d' \
	-e '/	sl	/d' \
	-e '/ppp/d' \
	-e '/pty/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/uhci/d' \
	-e '/ohci/d' \
	-e '/usb/d' \
	-e '/ugen/d' \
	-e '/uhid/d' \
	-e '/ukbd/d' \
	-e '/ulpt/d' \
	-e '/umass/d' \
	-e '/ums/d' \
	-e '/urio/d' \
	-e '/uscanner/d' \
	-e '/aue/d' \
	-e '/cue/d' \
	-e '/kue/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'
fi

# reset maxusers to something lower
echo "maxusers	5"

echo "options  NFS_NOSERVER" 
echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
