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
	-e '/maxusers/d' \
	-e '/DEBUG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_ACL/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/PROCFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/COMPAT_FREEBSD4/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/DDB/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/WITNESS/d' \
	-e '/	atapifd	/d' \
	-e '/	atapist	/d' \
	-e '/	ch	/d' \
	-e '/	sa	/d' \
	-e '/	pass	/d' \
	-e '/	ses	/d' \
	-e '/	splash	/d' \
	-e '/	apm$/d' \
	-e '/	pmtimer$/d' \
	-e '/	ppc$/d' \
	-e '/	ppbus	/d' \
	-e '/	lpt	/d' \
	-e '/	plip	/d' \
	-e '/	ppi	/d' \
	-e '/	an	/d' \
	-e '/	awi	/d' \
	-e '/	wi	/d' \
	-e '/	random	/d' \
	-e '/	sl	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	gif	/d' \
	-e '/	faith	/d' \
	-e '/	pci$/d' \
	-e '/	adv	/d' \
	-e '/	ahc	/d' \
	-e '/	amd	/d' \
	-e '/	isp	/d' \
	-e '/	sym	/d' \
	-e '/	de	/d' \
	-e '/	em	/d' \
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
	-e '/	xl	/d'

	echo "options 	ATA_NOPCI" 

else

sed	-e 's/ident.*GENERIC/ident		BOOTMFS/g' \
	-e '/maxusers/d' \
	-e '/DEBUG/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_ACL/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/PROCFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/COMPAT_FREEBSD4/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/DDB/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/WITNESS/d' \
	-e '/	atapifd	/d' \
	-e '/	atapist	/d' \
	-e '/	ch	/d' \
	-e '/	sa	/d' \
	-e '/	pass	/d' \
	-e '/	ses	/d' \
	-e '/	splash	/d' \
	-e '/	apm$/d' \
	-e '/	pmtimer$/d' \
	-e '/	ppc$/d' \
	-e '/	ppbus	/d' \
	-e '/	lpt	/d' \
	-e '/	plip	/d' \
	-e '/	ppi	/d' \
	-e '/	an	/d' \
	-e '/	awi	/d' \
	-e '/	wi	/d' \
	-e '/	random	/d' \
	-e '/	sl	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	gif	/d' \
	-e '/	faith	/d'

fi

echo "options  NETGRAPH"
echo "options  NETGRAPH_ETHER"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"
echo "options  NO_COMPAT_FREEBSD4"

# reset maxusers to something lower
echo "maxusers	5"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
