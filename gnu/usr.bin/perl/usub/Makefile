PROG=	curseperl

# From perl
SRCS+= array.c cmd.c cons.c consarg.c doarg.c doio.c dolist.c dump.c
SRCS+= eval.c form.c hash.c perl.c perly.c regcomp.c regexec.c
SRCS+= stab.c str.c toke.c util.c

# Local to us.
SRCS+=	usersub.c curses.c

CFLAGS+=	-DDEBUGGING -I${.CURDIR}/../perl
LDADD+=         -lncurses -lmytinfo -lcrypt -lm
DPADD+=         ${LIBNCURSES} ${LIBMYTINFO} ${LIBCRYPT} ${LIBM}
CLEANFILES+=	curses.c
VPATH+=	${.CURDIR}/../perl
NOMAN=	yes

# If perl exists in none of these places, something is horribly wrong.
.if exists(${.OBJDIR}/../perl/perl)
PERL=${.OBJDIR}/../perl/perl
.elif !defined(PERL) && exists(${.CURDIR}/../perl/perl)
PERL=${.CURDIR}/../perl/perl
.else
PERL=	${DESTDIR}/usr/bin/perl
.endif

curses.c: curses.mus
	${PERL} ${.CURDIR}/mus ${.CURDIR}/curses.mus > curses.c

.include "../../Makefile.inc"
.include <bsd.prog.mk>
