#	@(#)Makefile	5.4 (Berkeley) 2/19/91

MAN1=	mkdep.1

.if (${MACHINE} == "hp300" || ${MACHINE} == "i386")
beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep
.else
beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.sh ${DESTDIR}/usr/bin/mkdep
.endif

.include <bsd.prog.mk>
