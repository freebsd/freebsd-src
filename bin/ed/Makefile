PROG=	ed
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=  ${BINDIR}/ed ${BINDIR}/red
MLINKS= ed.1 red.1

.if exists(/usr/lib/libcrypt.a)
CFLAGS+=-DDES
LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}
.endif

.include <bsd.prog.mk>
