#	$Id: Makefile,v 1.10 1994/12/18 15:15:10 jkh Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=  ${BINDIR}/ed ${BINDIR}/red
MLINKS= ed.1 red.1

.if !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
CFLAGS+=-DDES
LDADD+= -L${CRYPTOBJDIR} -ldescrypt
DPADD+= ${CRYPTOBJDIR}/libdescrypt.a
.endif

.include <bsd.prog.mk>
