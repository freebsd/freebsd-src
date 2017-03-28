# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI=	pure
.endif

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	objdump
SRCS=	objdump.c prdbg.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
LIBADD=	binutils opcodes bfd iberty

.include <bsd.prog.mk>
