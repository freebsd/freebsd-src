# $FreeBSD$
#
all depend lint tags:

FILES= values.h
NOOBJ=	noobj

beforeinstall:
	@${ECHO} installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		    ${DESTDIR}/usr/include/$$i; \
	done

.include <bsd.prog.mk>
