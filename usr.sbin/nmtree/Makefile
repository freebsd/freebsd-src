#	$FreeBSD$

.include <src.opts.mk>

.PATH: ${SRCTOP}/contrib/mtree

PROG=		mtree
MAN=		mtree.5 mtree.8
SRCS=		compare.c crc.c create.c excludes.c getid.c misc.c mtree.c \
		only.c spec.c specspec.c verify.c

CFLAGS+=	-I${SRCTOP}/contrib/mknod
.PATH:		${SRCTOP}/contrib/mknod
SRCS+=		pack_dev.c

CFLAGS+=	-I${SRCTOP}/lib/libnetbsd
LIBADD=		netbsd md util

LINKS=		${BINDIR}/mtree ${BINDIR}/nmtree
MLINKS=		mtree.8 nmtree.8

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.if defined(BOOTSTRAPPING)
# Linux glibc has a dummy lchmod that always fails. Don't fail due to
# the linker warning that it emits.
LD_FATAL_WARNINGS=no
.endif

.include <bsd.prog.mk>
