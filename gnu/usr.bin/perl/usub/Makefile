PROG=	curseperl

SRCS+= array.c cmd.c cons.c consarg.c 
SRCS+= doarg.c doio.c dolist.c dump.c
SRCS+= eval.c form.c hash.c malloc.c 
SRCS+= perl.c perly.c regcomp.c regexec.c
SRCS+= stab.c str.c toke.c util.c 
SRCS+=	usersub.c curses.c

CFLAGS+=	-DDEBUGGING -I${.CURDIR}/../perl
LDADD+= 	-lncurses -ltermcap -ltermlib -lcrypt -lm
DPADD+=		${LIBNCURSES} ${LIBTERMCAP} ${LIBTERMLIB} ${LIBCRYPT} ${LIBM}
CLEANFILES+=	curses.c
VPATH+=	${.CURDIR}/../perl
NOMAN=	yes

curses.c: curses.mus
	@if [ ! -x ${.CURDIR}/mus ]; then chmod +x ${.CURDIR}/mus; fi
	${.CURDIR}/mus ${.CURDIR}/curses.mus > curses.c

.include "../../Makefile.inc"
.include <bsd.prog.mk>
