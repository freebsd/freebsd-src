#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# XXX MISSING:		fsdb icheck ncheck
# XXX NEEDS CONVERSION:	scsiformat
SUBDIR= adjkerntz badsect clri disklabel dmesg dump dumpfs dumplfs \
	fsck ft ifconfig init mknod modload modunload mount mount_cd9660 \
	mount_fdesc mount_kernfs mount_lfs mount_nfs mount_null mount_portal \
	mount_procfs mount_umap mount_union mountd newfs newlfs nfsd nfsiod \
	nologin ping quotacheck reboot restore route savecore shutdown \
	slattach startslip swapon tunefs umount

.if	make(clean) || make(cleandir)
SUBDIR+=comcontrol fdisk mount_msdos
.elif	${MACHINE} == "i386"
SUBDIR+=comcontrol fdisk mount_msdos
.endif

.include <bsd.subdir.mk>
