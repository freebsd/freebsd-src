#	@(#)Makefile	8.2 (Berkeley) 4/3/94

MAN1=	pagesize.0

all pagesize: ${MAN1}

clean depend lint tags:

cleandir:
	rm -f ${MAN1}

install: maninstall
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pagesize.sh ${DESTDIR}/${BINDIR}/pagesize

.include <bsd.prog.mk>
.include <bsd.man.mk>
