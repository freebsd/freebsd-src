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

sed	-e '/	pty	/d' \
	-e '/	pass	/d' \
	-e '/	apm$/d' \
	-e '/	pmtimer$/d' \
	-e '/	ppp	/d' \
	-e '/	gif	/d' \
	-e '/	faith	/d' \
	-e '/	random	/d' \
	-e '/	splash$/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/MFS/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/DEBUG/d' \
	-e '/DDB/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/WITNESS/d' \
	-e '/	pci$/d' \
	-e '/	adv	/d' \
	-e '/	agp	/d' \
	-e '/	ahc	/d' \
	-e '/	amd	/d' \
	-e '/	isp	/d' \
	-e '/	sym	/d' \
	-e '/	ncr	/d' \
	-e '/	ch	/d' \
	-e '/	sa	/d' \
	-e '/	ses	/d' \
	-e '/	pcm/d' \
	-e '/	atapist	/d' \
	-e '/	lpt	/d' \
	-e '/	ppi	/d' \
	-e '/	de	/d' \
	-e '/	txp	/d' \
	-e '/	vx	/d' \
	-e '/	dc	/d' \
	-e '/	fxp	/d' \
	-e '/	pcn	/d' \
	-e '/	rl	/d' \
	-e '/	sf	/d' \
	-e '/	sis	/d' \
	-e '/	ste	/d' \
	-e '/	tl	/d' \
	-e '/	tx	/d' \
	-e '/	vr	/d' \
	-e '/	wb	/d' \
	-e '/	xl	/d' \
	-e '/	ugen	/d' \
	-e '/	uhid	/d' \
	-e '/	ulpt	/d' \
	-e '/	urio	/d' \
	-e '/	uscanner	/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

else

sed	-e '/	pty	/d' \
	-e '/	pass	/d' \
	-e '/	apm$/d' \
	-e '/	pmtimer$/d' \
	-e '/	ppp	/d' \
	-e '/	gif	/d' \
	-e '/	faith	/d' \
	-e '/	random	/d' \
	-e '/	splash$/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/MFS/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
 	-e '/DEBUG/d' \
 	-e '/DDB/d' \
 	-e '/INVARIANTS/d' \
 	-e '/INVARIANT_SUPPORT/d' \
 	-e '/WITNESS/d' \
	-e '/	ncr	/d' \
	-e '/	pcm/d' \
	-e '/	agp	/d' \
	-e '/	atapist	/d' \
	-e '/	lpt	/d' \
	-e '/	ppi	/d' \
	-e '/	txp	/d' \
	-e '/	sf	/d' \
	-e '/	ste	/d' \
	-e '/	ugen	/d' \
	-e '/	uhid	/d' \
	-e '/	ulpt	/d' \
	-e '/	urio	/d' \
	-e '/	uscanner	/d' \
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
