#	from: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.1.1.1 1994/09/30 14:49:53 csgr Exp $

FILES=	des.h kparse.h krb.h krb_db.h

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
