#	@(#)Makefile	8.5 (Berkeley) 3/31/94
# $FreeBSD$

.include <bsd.own.mk>

# XXX MISSING:		icheck ncheck

SUBDIR=adjkerntz \
	badsect \
	camcontrol \
	clri \
	comcontrol \
	conscontrol \
	ddb \
	devfs \
	dhclient \
	dmesg \
	dump \
	dumpfs \
	dumpon \
	etherswitchcfg \
	ffsinfo \
	fsck \
	fsck_ffs \
	fsck_msdosfs \
	fsdb \
	fsirand \
	gbde \
	geom \
	ggate \
	growfs \
	gvinum \
	ifconfig \
	init \
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
	mount_fusefs \
	mount_msdosfs \
	mount_nfs \
	mount_nullfs \
	mount_udf \
	mount_unionfs \
	newfs \
	newfs_msdos \
	nfsiod \
	nos-tun \
	ping \
	rcorder \
	reboot \
	recoverdisk \
	resolvconf \
	restore \
	route \
	savecore \
	setkey \
	shutdown \
	spppcontrol \
	swapon \
	sysctl \
	tunefs \
	umount \

.if ${MK_ATM} != "no"
SUBDIR+=	atm
.endif
SUBDIR.${MK_ZFS}+=	zfsbootcfg

.if ${MK_CCD} != "no"
SUBDIR+=	ccdconfig
.endif

.if ${MK_CXX} != "no"
SUBDIR+=	devd
.endif

.if ${MK_HAST} != "no"
SUBDIR+=	hastctl
SUBDIR+=	hastd
.endif

.if ${MK_INET6} != "no"
SUBDIR+=	ping6
SUBDIR+=	rtsol
.endif
	
.if ${MK_IPFILTER} != "no"
SUBDIR+=	ipf
.endif

.if ${MK_IPFW} != "no"
SUBDIR+=	ipfw
SUBDIR+=	natd
.endif

.if ${MK_ISCSI} != "no"
SUBDIR+=	iscontrol
.endif

.if ${MK_NAND} != "no"
SUBDIR+=	nandfs
SUBDIR+=	newfs_nandfs
.endif

.if ${MK_PF} != "no"
SUBDIR+=	pfctl
SUBDIR+=	pflogd
.endif

.if ${MK_QUOTAS} != "no"
SUBDIR+=	quotacheck
.endif

.if ${MK_ROUTED} != "no"
SUBDIR+=	routed
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
