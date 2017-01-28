# $FreeBSD$

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if ${MK_OPENSSL} != "no" && ${MK_ED_CRYPTO} != "no"
CFLAGS+=-DDES
LIBADD=	crypto
.endif

NO_WERROR=	# Surpress -Wclobbered error in base gcc

.include <bsd.prog.mk>
