#	$FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../contrib/mtree

PROG=		mtree
MAN=		mtree.5 mtree.8
SRCS=		compare.c crc.c create.c excludes.c getid.c misc.c mtree.c \
		only.c spec.c specspec.c verify.c

CFLAGS+=	-I${.CURDIR}/../../contrib/mknod
.PATH:		${.CURDIR}/../../contrib/mknod
SRCS+=		pack_dev.c

CFLAGS+=	-I${.CURDIR}/../../lib/libnetbsd
LIBADD=		netbsd md util

LINKS=		${BINDIR}/mtree ${BINDIR}/nmtree
MLINKS=		mtree.8 nmtree.8

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
