#!/bin/sh
#
# $FreeBSD$
#

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
	-e '/PSEUDOFS/d' \
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
	-e '/AHC_REG_PRETTY_PRINT/d' \
	-e '/AHD_REG_PRETTY_PRINT/d' \
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
	-e '/	ses	/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

echo "options  NETGRAPH"
echo "options  NETGRAPH_ETHER"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"

# reset maxusers to something lower
echo "maxusers	5"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
