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

sed	-e 's/ident.*GENERIC/ident		BOOTMFS/g' \
	-e '/COMPAT_FREEBSD4/d' \
	-e '/DDB/d' \
	-e '/DEBUG/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/KTRACE/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/PROCFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/SOFTUPDATES/d' \
	-e '/SYSV/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/WITNESS/d' \
	-e '/	adv	/d' \
	-e '/	ahc	/d' \
	-e '/	amd	/d' \
	-e '/	an	/d' \
	-e '/	atapifd	/d' \
	-e '/	atapist	/d' \
	-e '/	awi	/d' \
	-e '/	ch	/d' \
	-e '/	dc	/d' \
	-e '/	de	/d' \
	-e '/	em	/d' \
	-e '/	faith	/d' \
	-e '/	fxp	/d' \
	-e '/	gif	/d' \
	-e '/	isp	/d' \
	-e '/	lpt	/d' \
	-e '/	pass	/d' \
	-e '/	pci$/d' \
	-e '/	pcn	/d' \
	-e '/	plip	/d' \
	-e '/	ppbus	/d' \
	-e '/	ppc$/d' \
	-e '/	ppi	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	random	/d' \
	-e '/	rl	/d' \
	-e '/	sa	/d' \
	-e '/	ses	/d' \
	-e '/	sf	/d' \
	-e '/	sis	/d' \
	-e '/	sl	/d' \
	-e '/	splash	/d' \
	-e '/	ste	/d' \
	-e '/	sym	/d' \
	-e '/	tl	/d' \
	-e '/	tx	/d' \
	-e '/	txp	/d' \
	-e '/	vr	/d' \
	-e '/	vx	/d' \
	-e '/	wb	/d' \
	-e '/	wi	/d' \
	-e '/	xl	/d'

	echo "options 	ATA_NOPCI" 

else

sed	-e 's/ident.*GENERIC/ident		BOOTMFS/g' \
	-e '/COMPAT_FREEBSD4/d' \
	-e '/DDB/d' \
	-e '/DEBUG/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/KTRACE/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/PROCFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/SOFTUPDATES/d' \
	-e '/SYSV/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/WITNESS/d' \
	-e '/	atapifd	/d' \
	-e '/	atapist	/d' \
	-e '/	ch	/d' \
	-e '/	faith	/d' \
	-e '/	gif	/d' \
	-e '/	lpt	/d' \
	-e '/	pass	/d' \
	-e '/	plip	/d' \
	-e '/	ppbus	/d' \
	-e '/	ppc$/d' \
	-e '/	ppi	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	random	/d' \
	-e '/	sa	/d' \
	-e '/	ses	/d' \
	-e '/	sl	/d' \
	-e '/	splash	/d'

fi

echo "options  NETGRAPH"
echo "options  NETGRAPH_ETHER"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
