#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	ranlib
SRCS=	archive.c build.c misc.c ranlib.c touch.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../ar
MAN1=	ranlib.0
VPATH=	${.CURDIR}/../ar

# XXX BROKEN: manual pages
# XXX CLEANFILES=ranlib.5.0

# XXX ranlib.0: ranlib.5.0

# XXX afterinstall:
# XXX 	install -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} ranlib.5.0 \
# XXX 	    ${DESTDIR}${MANDIR}5/ranlib.0

.include <bsd.prog.mk>
