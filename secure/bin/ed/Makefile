#	$Id: Makefile,v 1.9 1994/10/29 10:13:34 phk Exp $

PROG=	ed
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
.PATH:	${.CURDIR}/../../../bin/ed
LINKS=  ${BINDIR}/ed ${BINDIR}/red
NOMAN=	true

CFLAGS+=-DDES
LDADD+= -L../../lib/libdescrypt -ldescrypt
DPADD+= ../../lib/libcrypt/libdescrypt.a

.include <bsd.prog.mk>
