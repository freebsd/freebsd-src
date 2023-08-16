
.include <src.opts.mk>

PROG=	fstyp
SRCS=	apfs.c befs.c cd9660.c exfat.c ext2fs.c fstyp.c geli.c hammer.c	\
	hammer2.c hfsplus.c msdosfs.c ntfs.c ufs.c

.if ${MK_ZFS} != "no"
SRCS +=	zfs.c
.endif

MAN=	fstyp.8

.if ${MK_ICONV} == "yes"
CFLAGS+=	-DWITH_ICONV
.endif

.include <bsd.endian.mk>

.if ${TARGET_ENDIANNESS} == 1234
HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests
.endif

.if ${MK_ZFS} != "no"
IGNORE_PRAGMA=  YES

CFLAGS+= -DHAVE_ZFS
CFLAGS.zfs.c+= -DIN_BASE
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS.zfs.c+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS.zfs.c+= -DHAVE_ISSETUGID
CFLAGS.zfs.c+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS.zfs.c+= -Wno-cast-qual
.endif

.for src in ${SRCS}
.if ${src} != "zfs.c"
CFLAGS.${src}+=-I${SRCTOP}/sys
.endif
.endfor


LIBADD=	geom md ufs

.if ${MK_ZFS} != "no"
LIBADD+=nvpair zfs spl
.endif

.include <bsd.prog.mk>
