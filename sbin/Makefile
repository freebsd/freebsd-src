#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# XXX MISSING:		icheck ncheck
# XXX BROKEN:		mount_union

SUBDIR= adjkerntz badsect ccdconfig clri disklabel dmesg dset dump dumpfs \
	dumplfs dumpon fsck fsdb ifconfig init ipfw md5 mknod modload \
	modunload mount mount_cd9660 mount_ext2fs \
	mount_lfs mount_nfs mount_null mount_portal mount_std \
	mount_umap mountd newfs newlfs nfsd nfsiod \
	nologin ping quotacheck rdisc reboot restore route savecore scsi \
	scsiformat shutdown slattach startslip swapon tunefs umount

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
