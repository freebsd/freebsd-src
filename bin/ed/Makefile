#	Makefile,v 1.9 1994/10/29 10:13:34 phk Exp

PROG=	ed
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=  ${BINDIR}/ed ${BINDIR}/red
MLINKS= ed.1 red.1

.if !defined(NOCRYPT)
CFLAGS+=-DDES
LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}
.endif

.include <bsd.prog.mk>
