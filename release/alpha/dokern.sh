#!/bin/sh
#
# $FreeBSD$
#

sed	\
	-e '/DDB/d' \
	-e '/DEBUG/d' \
	-e '/DEC_KN8AE/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/KTRACE/d' \
	-e '/MSDOSFS/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/PROCFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/SMP/d' \
	-e '/SOFTUPDATES/d' \
	-e '/SYSV/d' \
	-e '/UFS_ACL/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/_KPOSIX_PRIORITY_SCHEDULING/d' \
	-e '/	atapifd	/d' \
	-e '/	atapist	/d' \
	-e '/	aue	/d' \
	-e '/	ch	/d' \
	-e '/	cue	/d' \
	-e '/	faith	/d' \
	-e '/	gif	/d' \
	-e '/	kue	/d' \
	-e '/	lpt	/d' \
	-e '/	ohci	/d' \
	-e '/	pass	/d' \
	-e '/	ppbus	/d' \
	-e '/	ppc$/d' \
	-e '/	ppi	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	random	/d' \
	-e '/	sa	/d' \
	-e '/	ses	/d' \
	-e '/	sf	/d' \
	-e '/	sis	/d' \
	-e '/	sl	/d' \
	-e '/	splash	/d' \
	-e '/	ste	/d' \
	-e '/	ugen	/d' \
	-e '/	uhci	/d' \
	-e '/	uhid	/d' \
	-e '/	ukbd	/d' \
	-e '/	ulpt	/d' \
	-e '/	umass	/d' \
	-e '/	ums	/d' \
	-e '/	usb	/d' \
	-e '/	wb	/d' \
	-e '/maxusers/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

# reset maxusers to something lower
echo "maxusers	2"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
