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
	conscontrol \
	dhclient \
	disklabel \
	dmesg \
	dump \
	dumpfs \
	dumpon \
	ffsinfo \
	fsck \
	fsck_ffs \
	fsck_ifs \
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

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=cxconfig \
	mount_nwfs \
	mount_smbfs \
	nextboot
.if ${MACHINE} == "pc98"
SUBDIR+=fdisk_pc98
.else
SUBDIR+=fdisk
.endif
.endif

.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=fdisk
.endif

.include <bsd.subdir.mk>
