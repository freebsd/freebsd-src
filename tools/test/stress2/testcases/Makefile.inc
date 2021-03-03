NO_OBJ=
LDFLAGS+=-L${.OBJDIR}/../../lib
CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/../include
LDADD+=-lstress

MAN=
