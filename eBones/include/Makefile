#	from: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1994/09/09 21:43:35 g89r4222 Exp $

FILES=	des.h kadm.h kparse.h krb.h krb_db.h

# mit-copyright.h kadm_err.h krb_err.h

NOOBJ=	noobj
NOMAN=	noman

all include clean cleandir depend lint tags:

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${FILES} ${DESTDIR}/usr/include/kerberosIV

.include <bsd.prog.mk>
