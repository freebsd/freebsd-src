#	$Id: Makefile,v 1.10.4.3 1996/11/24 09:31:31 joerg Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=  ${BINDIR}/ed ${BINDIR}/red
MLINKS= ed.1 red.1

.if exists(../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
CFLAGS+=-DDES
LDADD+= -L${CIPHEROBJDIR} -lcipher
DPADD+= ${CIPHEROBJDIR}/libcipher.a
.endif

.include <bsd.prog.mk>
