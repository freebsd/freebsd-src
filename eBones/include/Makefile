#	from: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/07/18 16:36:00 mark Exp $

FILES=	kparse.h krb.h krb_db.h

# mit-copyright.h kadm_err.h krb_err.h

NOOBJ=	noobj
NOMAN=	noman

all include clean cleandir depend lint tags:

beforeinstall:
	-cd ${.CURDIR}; for file in ${FILES}; do \
		cmp -s $$file ${DESTDIR}/usr/include/kerberosIV/$$file || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$file \
		${DESTDIR}/usr/include/kerberosIV; done

.include <bsd.prog.mk>
