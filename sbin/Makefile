#	@(#)Makefile	8.5 (Berkeley) 3/31/94
#	$Id: Makefile,v 1.55 1998/09/15 10:25:04 gibbs Exp $

# XXX MISSING:		icheck ncheck

SUBDIR=	atm \
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
	umount

.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=adjkerntz
.endif

.include <bsd.subdir.mk>
