# $FreeBSD$

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if ${MK_OPENSSL} != "no" && ${MK_ED_CRYPTO} != "no"
CFLAGS+=-DDES -I/usr/include/private/openssl
LIBADD=	crypto
.endif

.include <bsd.prog.mk>
