#	@(#)Makefile	8.5 (Berkeley) 3/31/94
#	$Id: Makefile,v 1.62.2.2 1999/03/18 06:18:11 jkh Exp $

# XXX MISSING:		icheck ncheck

SUBDIR=	adjkerntz \
	atm \
	badsect \
	camcontrol \
	ccdconfig \
	clri \
	dhclient \
	disklabel \
	dmesg \
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
	kget \
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
	natd \
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
	spppcontrol \
	startslip \
	swapon \
	tunefs \
	umount \
	vinum

.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
