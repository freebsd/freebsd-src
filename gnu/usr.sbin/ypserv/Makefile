# $Id: Makefile,v 1.3 1995/02/03 03:41:38 wpaul Exp $
# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	ypserv
SRCS=	dnslookup.c yp_svc.c yp_xdr.c server.c

CFLAGS+=-Wall -DTCP_WRAPPER=0 -DTCPW_FACILITY=LOG_AUTH
CFLAGS+=-DINSTDIR='"/usr/libexec"'

MAN8=

afterinstall: /var/yp/Makefile /usr/libexec/mknetid

/var/yp/Makefile: ${.CURDIR}/Makefile.yp
        @if [ ! -d /var/yp ]; then mkdir /var/yp; fi
	install -c -o bin -g bin -m 444 ${.CURDIR}/Makefile.yp /var/yp/Makefile

/usr/libexec/mknetid: ${.CURDIR}/mknetid
	install -c -o bin -g bin -m 555 ${.CURDIR}/mknetid /usr/libexec/mknetid

.include <bsd.prog.mk>
