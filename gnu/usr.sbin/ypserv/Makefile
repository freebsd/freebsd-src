# $Id: Makefile,v 1.6.4.1 1995/08/25 11:18:34 davidg Exp $
# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	ypserv
SRCS=	dnslookup.c svc_run.c yp_svc.c yp_xdr.c server.c

CFLAGS+=-Wall -DTCP_WRAPPER=0 -DTCPW_FACILITY=LOG_AUTH
CFLAGS+=-DMAX_CHILDREN=20 -DINSTDIR='"/usr/libexec"'

MAN8= ypserv.8

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/Makefile.yp \
		${DESTDIR}/var/yp/Makefile.dist
	@if [ ! -f ${DESTDIR}/var/yp/Makefile.dist ]; then \
		ln -s   ${DESTDIR}/var/yp/Makefile.dist \
			${DESTDIR}/var/yp/Makefile; fi
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mknetid \
		${DESTDIR}/usr/libexec/mknetid

.include <bsd.prog.mk>
