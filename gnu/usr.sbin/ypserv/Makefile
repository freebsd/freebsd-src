# $Id: Makefile,v 1.6 1995/02/15 04:33:52 wpaul Exp $
# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	ypserv
SRCS=	dnslookup.c svc_run.c yp_svc.c yp_xdr.c server.c

CFLAGS+=-Wall -DTCP_WRAPPER=0 -DTCPW_FACILITY=LOG_AUTH
CFLAGS+=-DMAX_CHILDREN=20 -DINSTDIR='"/usr/libexec"'

MAN8= ypserv.8

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/Makefile.yp \
		${DESTDIR}/var/yp/Makefile
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mknetid \
		${DESTDIR}/usr/libexec/mknetid

.include <bsd.prog.mk>
