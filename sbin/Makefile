#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# XXX MISSING:		icheck ncheck

SUBDIR= adjkerntz badsect ccdconfig clri disklabel dmesg dset dump dumpfs \
	dumplfs dumpon fsck fsdb fsirand ifconfig init ipfw \
	kldload kldstat kldunload md5 mknod modload \
	modunload mount mount_cd9660 mount_ext2fs \
	mount_lfs mount_nfs mount_null mount_portal mount_std \
	mount_umap mount_union mountd newfs newlfs nfsd nfsiod \
	nologin ping quotacheck reboot restore route routed savecore scsi \
	scsiformat shutdown slattach startslip spppcontrol swapon tunefs \
	umount

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
