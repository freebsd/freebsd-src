#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# XXX MISSING:		fsdb icheck ncheck

SUBDIR= adjkerntz badsect clri disklabel dmesg dset dump dumpfs \
	dumplfs dumpon fsck ifconfig init ipfw md5 mknod modload \
	modunload mount mount_cd9660 mount_fdesc mount_kernfs \
	mount_lfs mount_nfs mount_null mount_portal mount_procfs \
	mount_umap mount_union mountd newfs newlfs nfsd nfsiod \
	nologin ping quotacheck reboot restore route savecore scsi \
	scsiformat shutdown slattach startslip swapon tunefs umount

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
