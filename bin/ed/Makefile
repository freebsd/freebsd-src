#	$Id: Makefile,v 1.13.2.1 1998/07/15 02:01:22 jkh Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
CFLAGS+=-DDES
LDADD+= -L${CIPHEROBJDIR} -lcipher
DPADD+= ${CIPHEROBJDIR}/libcipher.a
.endif

.include <bsd.prog.mk>
