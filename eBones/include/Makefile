#	from: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.7 1996/07/30 21:15:58 markm Exp $

FILES=	kparse.h krb.h krb_db.h

NOOBJ=	noobj
NOMAN=	noman
CFLAGS+=	KERBEROS

all include clean cleandir depend lint tags:

beforeinstall:
.for i in ${FILES}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/$i \
		${DESTDIR}/usr/include/kerberosIV/$i
.endfor

.include <bsd.prog.mk>
