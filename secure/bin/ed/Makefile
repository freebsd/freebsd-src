#	$Id: Makefile,v 1.2 1994/11/21 02:58:43 phk Exp $

PROG=	ed
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
.PATH:	${.CURDIR}/../../../bin/ed
LINKS=  ${BINDIR}/ed ${BINDIR}/red
NOMAN=	true

CFLAGS+=-DDES
LDADD+= -L${CRYPTOBJDIR} -ldescrypt
DPADD+= ${CRYPTOBJDIR}/libdescrypt.a

.include <bsd.prog.mk>
