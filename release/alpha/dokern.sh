#!/bin/sh
#
# $FreeBSD$
#

#	XXX sort by order in GENERIC, not alphabetical

sed	\
	-e 's/ident.*GENERIC/ident		BOOTMFS/g' \
	-e '/DEBUG/d' \
	-e '/DEC_KN8AE/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_ACL/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/MSDOSFS/d' \
	-e '/PROCFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/_KPOSIX_PRIORITY_SCHEDULING/d' \
	-e '/DDB/d' \
	-e '/INVARIANTS/d' \
	-e '/INVARIANT_SUPPORT/d' \
	-e '/WITNESS/d' \
	-e '/SMP/d' \
	-e '/	atapifd	/d' \
	-e '/	atapist	/d' \
	-e '/	mpt	/d' \
	-e '/	ch	/d' \
	-e '/	pass	/d' \
	-e '/	sa	/d' \
	-e '/	ses	/d' \
	-e '/	splash	/d' \
	-e '/	ppc$/d' \
	-e '/	ppbus	/d' \
	-e '/	lpt	/d' \
	-e '/	ppi	/d' \
	-e '/	random	/d' \
	-e '/	sl	/d' \
	-e '/	ppp	/d' \
	-e '/	pty	/d' \
	-e '/	faith	/d' \
	-e '/	uhci	/d' \
	-e '/	ohci	/d' \
	-e '/	usb	/d' \
	-e '/	ugen	/d' \
	-e '/	uhid	/d' \
	-e '/	ukbd	/d' \
	-e '/	ulpt	/d' \
	-e '/	umass	/d' \
	-e '/	ums	/d' \
	-e '/	aue	/d' \
	-e '/	axe	/d' \
	-e '/	cue	/d' \
	-e '/	kue	/d' \
	-e '/	firewire	/d' \
	-e '/	sbp	/d' \
	-e '/	fwe	/d' \

#	XXX: blank line above intentional so lines may be added below the last
#	entry with ease.

echo "options  MUTEX_NOINLINE"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
