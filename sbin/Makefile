#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# XXX MISSING:		fsdb icheck ncheck
# XXX NEEDS CONVERSION:	scsiformat (make a script using sbin/scsi)
SUBDIR= adjkerntz badsect clri disklabel dmesg dset dump dumpfs dumplfs \
	fsck ifconfig init ipfw md5 mknod modload modunload mount mount_cd9660 \
	mount_fdesc mount_kernfs mount_lfs mount_nfs mount_null mount_portal \
	mount_procfs mount_umap mount_union mountd newfs newlfs nfsd nfsiod \
	nologin ping quotacheck reboot restore route savecore scsi shutdown \
	slattach startslip swapon sysinstall tunefs umount

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
