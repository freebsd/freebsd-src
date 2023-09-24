
ZFSTOP= ${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/scripts

PACKAGE=	zfs
SCRIPTS=	zfs_prepare_disk 
SCRIPTSDIR=	${LIBEXECDIR}/zfs

.PATH: ${ZFSTOP}/man/man8
MAN=		zfs_prepare_disk.8

CLEANFILES+=	zfs_prepare_disk.8

zfs_prepare_disk.8: zfs_prepare_disk.8.in
	sed ${MAN_SUB} ${.ALLSRC} >${.TARGET}

MAN_SUB+= -e 's|@zfsexecdir@|${LIBEXECDIR}/zfs|g'

.include <bsd.prog.mk>
