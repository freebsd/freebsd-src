#	$FreeBSD$

PROG=	makefs

CFLAGS+=-I${.CURDIR}

SRCS=	cd9660.c ffs.c \
	getid.c \
	makefs.c \
	mtree.c \
	walk.c
MAN=	makefs.8

WARNS?=	2

.include "${.CURDIR}/cd9660/Makefile.inc"
.include "${.CURDIR}/ffs/Makefile.inc"
.include "${.CURDIR}/compat/Makefile.inc"

CFLAGS+=-DHAVE_STRUCT_STAT_ST_FLAGS=1
CFLAGS+=-DHAVE_STRUCT_STAT_ST_GEN=1

.PATH:	${.CURDIR}/../../contrib/mtree
CFLAGS+=-I${.CURDIR}/../../contrib/mtree
SRCS+=	misc.c spec.c

.PATH:	${.CURDIR}/../../contrib/mknod
CFLAGS+=-I${.CURDIR}/../../contrib/mknod
SRCS+=	pack_dev.c

.PATH:	${.CURDIR}/../../sys/ufs/ffs
SRCS+=	ffs_tables.c

CFLAGS+=-I${.CURDIR}/../../lib/libnetbsd
DPADD=  ${LIBSBUF}
LDADD=  -lsbuf -lutil

LIBNETBSDDIR=   ${.OBJDIR}/../../lib/libnetbsd
LIBNETBSD=      ${LIBNETBSDDIR}/libnetbsd.a
DPADD+=         ${LIBNETBSD}
LDADD+=         ${LIBNETBSD}

.include <bsd.prog.mk>
