#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# XXX MISSING:		icheck ncheck

SUBDIR= disklabel dmesg dset dump dumpfs \
	dumpon fsck fsdb fsirand ifconfig init ipfw \
	md5 mknod \
	mount mount_cd9660 mount_ext2fs \
	mount_nfs mount_null mount_portal mount_std \
	mount_umap mount_union mountd newfs nfsd nfsiod \
	noc-tun nologin quotacheck reboot restore route routed \
	scsiformat shutdown slattach startslip spppcontrol tunefs \
	umount

SUBDIR+=ipf

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=adjkerntz badsect ccdconfig clri kldload kldstat kldunload ping
SUBDIR+=modload modunload savecore scsi swapon
.endif

.include <bsd.subdir.mk>
