#	$Id$

PROG= chkey
SRCS= chkey.c update.c generic.c

.PATH: ${.CURDIR}/../newkey

MAN1= chkey.1

CFLAGS+= -DYP

LDADD+= -lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
