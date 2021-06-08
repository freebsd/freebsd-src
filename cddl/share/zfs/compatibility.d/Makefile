# $FreeBSD$

ZFSTOP= ${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/zpool/compatibility.d

FILES= \
	compat-2018 \
	compat-2019 \
	compat-2020 \
	compat-2021 \
	freebsd-11.0 \
	freebsd-11.2 \
	freebsd-11.3 \
	freenas-9.10.2 \
	grub2 \
	openzfsonosx-1.7.0 \
	openzfsonosx-1.8.1 \
	openzfsonosx-1.9.3 \
	openzfs-2.0-freebsd \
	openzfs-2.0-linux \
	openzfs-2.1-freebsd \
	openzfs-2.1-linux \
	zol-0.6.1 \
	zol-0.6.4 \
	zol-0.6.5 \
	zol-0.7 \
	zol-0.8

FILESDIR= ${SHAREDIR}/zfs/compatibility.d

LINKS= \
	${FILESDIR}/compat-2018		${FILESDIR}/2018 \
	${FILESDIR}/compat-2019		${FILESDIR}/2019 \
	${FILESDIR}/compat-2020		${FILESDIR}/2020 \
	${FILESDIR}/compat-2021		${FILESDIR}/2021 \
	${FILESDIR}/freebsd-11.0	${FILESDIR}/freebsd-11.1 \
	${FILESDIR}/freebsd-11.0	${FILESDIR}/freenas-11.0 \
	${FILESDIR}/freebsd-11.2	${FILESDIR}/freenas-11.2 \
	${FILESDIR}/freebsd-11.3	${FILESDIR}/freebsd-11.4 \
	${FILESDIR}/freebsd-11.3	${FILESDIR}/freebsd-12.0 \
	${FILESDIR}/freebsd-11.3	${FILESDIR}/freebsd-12.1 \
	${FILESDIR}/freebsd-11.3	${FILESDIR}/freebsd-12.2 \
	${FILESDIR}/freebsd-11.3	${FILESDIR}/freenas-11.3 \
	${FILESDIR}/freenas-11.0	${FILESDIR}/freenas-11.1 \
	${FILESDIR}/openzfsonosx-1.9.3	${FILESDIR}/openzfsonosx-1.9.4 \
	${FILESDIR}/openzfs-2.0-freebsd	${FILESDIR}/truenas-12.0 \
	${FILESDIR}/zol-0.7		${FILESDIR}/ubuntu-18.04 \
	${FILESDIR}/zol-0.8		${FILESDIR}/ubuntu-20.04

LINKMODE= ${NOBINMODE}

.include <bsd.prog.mk>
