#	@(#)Makefile	8.5 (Berkeley) 3/31/94
#	$Id: Makefile,v 1.57 1998/09/28 05:38:52 grog Exp $

# XXX MISSING:		icheck ncheck

SUBDIR=	atm \
	adjkerntz \
	badsect \
	camcontrol \
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
	shutdown \
	slattach \
	startslip \
	spppcontrol \
	swapon \
	tunefs \
	umount \
	vinum

.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
