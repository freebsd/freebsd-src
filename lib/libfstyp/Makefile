# $FreeBSD$

.include <src.opts.mk>

LIB=		fstyp
SHLIB_MAJOR=	0
INCS= 		fstyp.h	
MAN=		
MLINKS=		

CFLAGS+=	-I${.CURDIR}

SRCS= apfs.c cd9660.c exfat.c ext2fs.c fstyp.c hammer.c	\
	hammer2.c hfsplus.c msdosfs.c ntfs.c ufs.c  geli.c

.if ${MK_ZFS} != "no"
SRCS +=	zfs.c
.endif

WARNS?=	2

.if ${MK_ICONV} == "yes"
CFLAGS+=	-DWITH_ICONV
.endif

.if ${MK_ZFS} != "no"
IGNORE_PRAGMA=  YES

WARNS?=	0
CFLAGS+= -DHAVE_ZFS
CFLAGS.zfs.c+= -DIN_BASE
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS.zfs.c+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS.zfs.c+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS.zfs.c+= -DHAVE_ISSETUGID
CFLAGS.zfs.c+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
.endif

CFLAGS+= -I${SRCTOP}/sys/lib/libufs/

.for src in ${SRCS}
.if ${src} != "zfs.c"
CFLAGS.${src}+=-I${SRCTOP}/sys
.endif
.endfor

LIBADD=	geom md ufs 

.if ${MK_ZFS} != "no"
LIBADD+=nvpair zfs spl
.endif

.include <bsd.lib.mk>
