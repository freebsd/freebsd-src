#
# $Id$
#

PROG= propdelay
LIBADD= -L${.CURDIR}/../lib -lntp -lm
DPADD= ${.CURDIR}/../lib/libntp.a

SRCS= propdelay.c
NOMAN=

install:

CLEANFILES+= chutest clktest chutest.o clktest.o

.include <bsd.prog.mk>
