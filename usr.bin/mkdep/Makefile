#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN1=	mkdep.0

.if (${MACHINE} == "hp300" || ${MACHINE} == "i386" || \
	${MACHINE} == "mips" || ${MACHINE} == "sparc")
beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep
.else
beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.sh ${DESTDIR}/usr/bin/mkdep
.endif

.include <bsd.prog.mk>
