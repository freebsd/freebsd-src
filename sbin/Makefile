#	@(#)Makefile	8.5 (Berkeley) 3/31/94
#	$Id: Makefile,v 1.47 1998/05/18 04:46:29 jb Exp $

# XXX MISSING:		icheck ncheck

SUBDIR= init md5 mknod nologin ping reboot route routed scsiformat shutdown

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=adjkerntz badsect ccdconfig clri disklabel dmesg dset dump dumpfs \
	dumpon fsck fsdb fsirand ifconfig ipf ipfw kldload kldstat \
	kldunload ldconfig modload modunload mount mount_cd9660 mount_ext2fs \
	mount_nfs mount_null mount_portal mount_std mount_umap \
	mount_union mountd newfs nfsd nfsiod nos-tun quotacheck \
	restore savecore scsi slattach startslip spppcontrol swapon \
	tunefs umount
.endif

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
