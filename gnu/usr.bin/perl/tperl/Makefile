#
#

PROG= tperl
LINKS= ${BINDIR}/tperl ${BINDIR}/tperl4.036

SRCS+= array.c cmd.c cons.c consarg.c
SRCS+= doarg.c doio.c dolist.c dump.c
SRCS+= eval.c form.c hash.c
SRCS+= perl.c perly.c regcomp.c regexec.c
SRCS+= stab.c str.c toke.c util.c
SRCS+= usersub.c
.PATH: ${.CURDIR}/../perl


CFLAGS+= -I${.CURDIR}/../perl -DTAINT
LDADD+= -lm
DPADD= ${LIBM}

LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}

MAN1=
MLINKS+= perl.1 tperl.1
MLINKS+= perl.1 taintperl.1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
