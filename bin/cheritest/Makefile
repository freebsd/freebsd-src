# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c cheritest_sandbox.S
NO_MAN=yes

#DPADD=  ${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=  -lcheri

.if defined(CHERI_CC) && defined(SYSROOT)
CC=	${CHERI_CC} --sysroot=${SYSROOT} -integrated-as

# XXXRW: Needed as Clang rejects -G0 when using $CC to link.
CC+=	-Qunused-arguments

# XXXRW: Until ELF types are right
LDFLAGS+=	-Wl,--no-warn-mismatch
.endif

NO_SHARED?=	YES

.include <bsd.prog.mk>
