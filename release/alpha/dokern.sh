#!/bin/sh
#
# $FreeBSD$
#

sed	\
	-e '/DEC_3000_300/d' \
	-e '/DEC_3000_500/d' \
	-e '/DEC_KN8AE/d' \
	-e '/SOFTUPDATES/d' \
	-e '/UFS_ACL/d' \
	-e '/UFS_DIRHASH/d' \
	-e '/MFS/d' \
	-e '/NFSSERVER/d' \
	-e '/NFS_ROOT/d' \
	-e '/MSDOSFS/d' \
	-e '/PSEUDOFS/d' \
	-e '/PROCFS/d' \
	-e '/KTRACE/d' \
	-e '/SYSV/d' \
	-e '/P1003_1B/d' \
	-e '/_KPOSIX_PRIORITY_SCHEDULING/d' \
 	-e '/DEBUG/d' \
 	-e '/DDB/d' \
 	-e '/INVARIANTS/d' \
 	-e '/INVARIANT_SUPPORT/d' \
 	-e '/WITNESS/d' \
 	-e '/AHC_REG_PRETTY_PRINT/d' \
 	-e '/AHD_REG_PRETTY_PRINT/d' \
 	-e '/SMP/d' \
	-e '/atapifd/d' \
	-e '/atapist/d' \
	-e '/	ncr/d' \
	-e '/	sa/d' \
	-e '/pass/d' \
	-e '/ses/d' \
	-e '/splash/d' \
	-e '/	apm/d' \
	-e '/pmtimer/d' \
	-e '/pcm/d' \
	-e '/ppc/d' \
	-e '/ppbus/d' \
	-e '/plip/d' \
	-e '/lpt/d' \
	-e '/ppi/d' \
	-e '/vpo/d' \
	-e '/	le	/d' \
	-e '/	xv	/d' \
	-e '/	sf	/d' \
	-e '/	sis	/d' \
	-e '/	ste	/d' \
	-e '/	wb	/d' \
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
	-e '/ch.*media/d' \
	-e 's/ident.*GENERIC/ident		BOOTMFS/g'

# reset maxusers to something lower
echo "maxusers	2"

echo "options  SCSI_NO_OP_STRINGS" 
echo "options  SCSI_NO_SENSE_STRINGS"
