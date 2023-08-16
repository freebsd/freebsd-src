
ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/zpool_influxdb
.PATH: ${ZFSTOP}/man/man8

PROG=	zpool_influxdb
MAN=	zpool_influxdb.8
BINDIR?=	/usr/libexec/zfs

WARNS?=	0
CSTD=	c99

CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h \
	-DSYSCONFDIR=\"/etc\" \
	-DPKGDATADIR=\"/usr/share/zfs\"

LIBADD=	nvpair spl zfs

.include <bsd.prog.mk>
