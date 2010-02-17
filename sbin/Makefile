#	@(#)Makefile	8.5 (Berkeley) 3/31/94
# $FreeBSD$

.include <bsd.own.mk>

# XXX MISSING:		icheck ncheck

_SUBDIR=adjkerntz \
	atacontrol \
	badsect \
	camcontrol \
	ccdconfig \
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
	iscontrol \
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
	mount_msdosfs \
	mount_nfs \
	mount_ntfs \
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
_SUBDIR += atm
.endif

.if ${MK_CXX} != "no"
_SUBDIR += devd
.endif

.if ${MK_IPFILTER} != "no"
_SUBDIR += ipf
.endif

.if ${MK_IPFW} != "no"
_SUBDIR += ipfw
_SUBDIR += natd
.endif

.if ${MK_PF} != "no"
_SUBDIR += pfctl
_SUBDIR += pflogd
.endif

.if ${MK_INET6} != "no"
_SUBDIR += ping6
_SUBDIR += rtsol
.endif

.if ${MK_QUOTAS} != "no"
_SUBDIR += quotacheck
.endif

.if ${MK_ROUTED} != "no"
_SUBDIR += routed
.endif

.include <bsd.arch.inc.mk>

SUBDIR=${_SUBDIR:O}

.include <bsd.subdir.mk>
