#	$Id: Makefile,v 1.1 1994/11/14 20:45:26 phk Exp $

PROG=	ed
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
.PATH:	${.CURDIR}/../../../bin/ed
LINKS=  ${BINDIR}/ed ${BINDIR}/red
NOMAN=	true

CFLAGS+=-DDES
LDADD+= -L../../lib/libcrypt -ldescrypt
DPADD+= ../../lib/libcrypt/libdescrypt.a

.include <bsd.prog.mk>
