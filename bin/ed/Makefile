#	$Id: Makefile,v 1.11 1995/10/01 14:17:29 markm Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=  ${BINDIR}/ed ${BINDIR}/red
MLINKS= ed.1 red.1

.if exists(../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
CFLAGS+=-DDES
LDADD+= -L${CRYPTOBJDIR} -ldescrypt
DPADD+= ${CRYPTOBJDIR}/libdescrypt.a
.endif

.include <bsd.prog.mk>
