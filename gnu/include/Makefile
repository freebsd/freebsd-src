#	$Id: Makefile,v 1.19 1994/11/01 09:19:50 pst Exp $
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
