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
	-e '/	fxp/d' \
	-e '/	tx/d' \
	-e '/	txp/d' \
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
	-e '/MATH_EMULATE/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/P1003_1B/d' \
	-e '/DEBUG/d' \
	-e '/DDB/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/WITNESS/d' \
	-e '/MSDOS/d' \
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
	-e '/SYSV/d' \
	-e '/SOFTUPDATES/d' \
	-e '/MFS/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
 	-e '/DEBUG/d' \
 	-e '/DDB/d' \
 	-e '/INVARIANTS/d' \
 	-e '/INVARIANT_SUPPORT/d' \
 	-e '/WITNESS/d' \
	-e '/	ncr/d' \
	-e '/pcm/d' \
	-e '/atapist/d' \
	-e '/wds/d' \
	-e '/lpt/d' \
	-e '/ppi/d' \
	-e '/	txp/d' \
	-e '/	sf/d' \
	-e '/	ste/d' \
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

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
