#	$Id: Makefile,v 1.1.6.2 1996/06/05 02:38:25 jkh Exp $
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
