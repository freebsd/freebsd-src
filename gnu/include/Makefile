#	$Id: Makefile,v 1.1 1994/11/08 00:47:02 ache Exp $
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
