#	from: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

FILES=	kparse.h krb.h krb_db.h

NOOBJ=	noobj
NOMAN=	noman

all include clean cleandir depend lint tags:

beforeinstall:
	-cd ${.CURDIR}; for file in ${FILES}; do \
		cmp -s $$file ${DESTDIR}/usr/include/kerberosIV/$$file || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$file \
		${DESTDIR}/usr/include/kerberosIV; done

.include <bsd.prog.mk>
