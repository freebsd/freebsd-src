#!/bin/sh
#
# $FreeBSD$
#

sed	\
	-e '/AHC_REG_PRETTY_PRINT/d' \
	-e '/AHD_REG_PRETTY_PRINT/d' \
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
	-e '/_KPOSIX_PRIORITY_SCHEDULING/d' \
	-e '/	atapist	/d' \
	-e '/	faith	/d' \
	-e '/	gif	/d' \
	-e '/	lpt	/d' \
	-e '/	pass	/d' \
	-e '/	pmtimer$/d' \
	-e '/	ppi	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	random	/d' \
	-e '/	ses	/d' \
	-e '/	splash	/d' \
	-e '/	ugen	/d' \
	-e '/	uhid	/d' \
	-e '/	ulpt	/d' \
	-e '/	urio	/d' \
	-e '/	uscanner	/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

echo "options  MUTEX_NOINLINE"

echo "options  NETGRAPH"
echo "options  NETGRAPH_ETHER"
echo "options  NETGRAPH_PPPOE"
echo "options  NETGRAPH_SOCKET"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
