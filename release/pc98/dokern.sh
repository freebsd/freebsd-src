#!/bin/sh
#
# $FreeBSD$
#

if [ $# -lt 1 ]; then
	FDSIZE=NORMAL
else
	FDSIZE=$1
fi

if [ "${FDSIZE}" = "SMALL" ]; then

sed	-e '/	pci$/d' \
	-e '/	adv/d' \
	-e '/	ahc/d' \
	-e '/	amd/d' \
	-e '/	isp/d' \
	-e '/	ncr/d' \
	-e '/	sym/d' \
	-e '/	de/d' \
	-e '/	em/d' \
	-e '/	fxp/d' \
	-e '/	tx/d' \
	-e '/	bge/d' \
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
	-e '/ppp/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/P1003_1B/d' \
	-e '/_KPOSIX_PRIORITY_SCHEDULING/d' \
	-e '/RANDOMDEV/d' \
	-e '/AHC_REG_PRETTY_PRINT/d' \
	-e '/AHD_REG_PRETTY_PRINT/d' \
	-e '/atapist/d' \
	-e '/lpt/d' \
	-e '/ppi/d' \
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
	-e '/ppp/d' \
	-e '/gif/d' \
	-e '/faith/d' \
	-e '/gzip/d' \
	-e '/splash/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSVMSG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/MFS/d' \
	-e '/NFS_ROOT/d' \
	-e '/RANDOMDEV/d' \
	-e '/AHC_REG_PRETTY_PRINT/d' \
	-e '/AHD_REG_PRETTY_PRINT/d' \
	-e '/	ncr/d' \
	-e '/atapist/d' \
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

# reset maxusers to something lower
echo "maxusers	5"

echo "options  NFS_NOSERVER" 
echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
