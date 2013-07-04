#	$FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/mtree

PROG=		nmtree
MAN=		nmtree.8
SRCS=		compare.c crc.c create.c excludes.c getid.c misc.c mtree.c \
		only.c spec.c specspec.c verify.c
LDADD+=		-lmd -lutil

CFLAGS+=	-I${.CURDIR}/../../contrib/mknod
.PATH:		${.CURDIR}/../../contrib/mknod
SRCS+=		pack_dev.c

CFLAGS+=	-I${.CURDIR}/../../lib/libnetbsd
LIBNETBSDDIR=	${.OBJDIR}/../../lib/libnetbsd
LIBNETBSD=	${LIBNETBSDDIR}/libnetbsd.a
DPADD+=		${LIBNETBSD}
LDADD+=		${LIBNETBSD}

.if ${MK_NMTREE} != "no"
LINKS=		${BINDIR}/nmtree ${BINDIR}/mtree
MLINKS=		nmtree.8 mtree.8
.endif

CLEANFILES+=	nmtree.8

nmtree.8: mtree.8
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
