# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_PURE} == "yes"
WANT_CHERI=	pure
.endif


.include "../Makefile.inc"
.include "../Makefile.fe"

PROG=	gcpp
SRCS+=	cppspec.c

.if ${MK_CLANG_IS_CC} == "no"
LINKS=	${BINDIR}/gcpp ${BINDIR}/cpp
MLINKS=	gcpp.1 cpp.1
.endif

.include <bsd.prog.mk>
