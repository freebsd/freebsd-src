#	@(#)Makefile	8.5 (Berkeley) 3/31/94
#	$Id: Makefile,v 1.53 1998/09/05 07:35:19 jb Exp $

# XXX MISSING:		icheck ncheck

SUBDIR= badsect \
	ccdconfig \
	clri \
	disklabel \
	dmesg \
	dset \
	dump \
	dumpfs \
	dumpon \
	fsck \
	fsdb \
	fsirand \
	ifconfig \
	init \
	ipf \
	ipfw \
	kldload \
	kldstat \
	kldunload \
	ldconfig \
	md5 \
	mknod \
	modload \
	modunload \
	mount \
	mount_cd9660 \
	mount_ext2fs \
	mount_nfs \
	mount_null \
	mount_portal \
	mount_std \
	mount_umap \
	mount_union \
	mountd \
	newfs \
	newfs_msdos \
	nfsd \
	nfsiod \
	nologin \
	nos-tun \
	ping \
	quotacheck \
	reboot \
	restore \
	route \
	routed \
	savecore \
	scsiformat \
	shutdown \
	slattach \
	startslip \
	spppcontrol \
	swapon \
	tunefs \
	umount

.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=adjkerntz \
	scsi
.endif

.include <bsd.subdir.mk>
