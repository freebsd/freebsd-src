#
#

PROG= suidperl
LINKS= ${BINDIR}/suidperl ${BINDIR}/sperl4.036

SRCS+= array.c cmd.c cons.c consarg.c
SRCS+= doarg.c doio.c dolist.c dump.c
SRCS+= eval.c form.c hash.c malloc.c
SRCS+= perl.c perly.c regcomp.c regexec.c
SRCS+= stab.c str.c toke.c util.c
SRCS+= usersub.c
.PATH: ${.CURDIR}/../perl


CFLAGS+= -I${.CURDIR}/../perl -DIAMSUID -DTAINT
LDADD= -lm
DPADD= ${LIBM}

LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}

MAN1=
MLINKS+= perl.1 suidperl.1

BINOWN= root
BINMODE=4111

.include "../../Makefile.inc"
.include <bsd.prog.mk>
