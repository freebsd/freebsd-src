#	@(#)Makefile	8.5 (Berkeley) 3/31/94
# $FreeBSD$

# XXX MISSING:		icheck ncheck

SUBDIR=	adjkerntz \
	atacontrol \
	atm \
	badsect \
	camcontrol \
	ccdconfig \
	clri \
	comcontrol \
	dhclient \
	disklabel \
	dmesg \
	dump \
	dumpfs \
	dumpon \
	ffsinfo \
	fsck \
	fsck_ifs \
	fsck_ffs \
	fsck_msdosfs \
	fsdb \
	fsirand \
	growfs \
	ifconfig \
	init \
	ip6fw \
	ipf \
	ipfstat \
	ipfw \
	ipmon \
	ipnat \
	kldconfig \
	kldload \
	kldstat \
	kldunload \
	ldconfig \
	md5 \
	mdconfig \
	mdmfs \
	mknod \
	mount \
	mount_cd9660 \
	mount_ext2fs \
	mount_ifs \
	mount_msdosfs \
	mount_nfs \
	mount_ntfs \
	mount_nullfs \
	mount_portalfs \
	mount_std \
	mount_umapfs \
	mount_unionfs \
	mountd \
	natd \
	newfs \
	newfs_msdos \
	nfsd \
	nfsiod \
	nologin \
	nos-tun \
	ping \
	ping6 \
	quotacheck \
	reboot \
	restore \
	route \
	routed \
	rtsol \
	savecore \
	shutdown \
	slattach \
	spppcontrol \
	startslip \
	sysctl \
	swapon \
	tunefs \
	umount \
	vinum

.if ${MACHINE_ARCH} == i386
SUBDIR+=	kget mount_nwfs
.endif

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
