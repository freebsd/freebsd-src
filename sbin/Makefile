#	@(#)Makefile	8.5 (Berkeley) 3/31/94
# $FreeBSD$

# XXX MISSING:		icheck ncheck

SUBDIR=	adjkerntz \
	atacontrol \
	badsect \
	bsdlabel \
	camcontrol \
	ccdconfig \
	clri \
	comcontrol \
	conscontrol \
	devfs \
	dhclient \
	dmesg \
	dump \
	dumpfs \
	dumpon \
	ffsinfo \
	fsck \
	fsck_ffs \
	fsck_msdosfs \
	fsdb \
	fsirand \
	gbde \
	growfs \
	ifconfig \
	init \
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
	mksnap_ffs \
	mount \
	mount_cd9660 \
	mount_ext2fs \
	mount_msdosfs \
	mount_nfs \
	mount_ntfs \
	mount_nullfs \
	mount_std \
	mount_udf \
	mount_umapfs \
	mount_unionfs \
	natd \
	newfs \
	newfs_msdos \
	nfsiod \
	nologin \
	nos-tun \
	ping \
	quotacheck \
	raidctl \
	rcorder \
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
	sunlabel \
	swapon \
	sysctl \
	tunefs \
	umount

.if !defined(NOATM)
SUBDIR+=atm
.endif

.if !defined(NO_CXX)
SUBDIR+=devd
.endif

.if !defined(NO_IPFILTER)
SUBDIR+=ipf \
	ipfs \
	ipfstat \
	ipmon \
	ipnat
.endif

.if !defined(NOINET6)
SUBDIR+=ip6fw \
	ping6
.endif

.if !defined(NO_VINUM)
SUBDIR+=vinum
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=cxconfig
.if ${MACHINE} == "pc98"
SUBDIR+=fdisk_pc98
.else
SUBDIR+=fdisk
.endif
.endif

.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=fdisk \
	gpt \
	mca
.endif

.if ${MACHINE_ARCH} == "amd64"
SUBDIR+=fdisk \
	gpt
.endif

.include <bsd.subdir.mk>
