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
	fsck_msdosfs \
	fsdb \
	fsirand \
	growfs \
	ifconfig \
	init \
	ip6fw \
	ipfw \
	kldconfig \
	kldload \
	kldstat \
	kldunload \
	ldconfig \
	md5 \
	mknod \
	mount \
	mount_cd9660 \
	mount_ext2fs \
	mount_msdos \
	mount_nfs \
	mount_ntfs \
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
	swapon \
	sysctl \
	tunefs \
	umount \
	vinum

.if !defined(NO_IPFILTER)
SUBDIR+=ipf \
	ipfs \
	ipfstat \
	ipmon \
	ipnat
.endif

.if ${MACHINE_ARCH} == i386
SUBDIR+=kget \
	mount_nwfs \
	mount_smbfs
.endif

.if exists(${.CURDIR}/${MACHINE})
SUBDIR+= ${MACHINE}
.endif

.include <bsd.subdir.mk>
