# $Id$
# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	ypserv
SRCS=	dnslookup.c yp_svc.c yp_xdr.c server.c

CFLAGS+=-Wall -DTCP_WRAPPER=0 -DTCPW_FACILITY=LOG_AUTH
CFLAGS+=-DINSTDIR='"/usr/libexec"'

BINOWN=	bin
BINMODE=555
BINDIR= /usr/sbin
MAN8=

afterinstall: /var/yp/Makefile /usr/libexec/mknetid

/var/yp/Makefile: Makefile.yp
	install -c -o bin -g bin -m 444 Makefile.yp /var/yp/Makefile

/usr/libexec/mknetid: mknetid
	install -c -o bin -g bin -m 555 mknetid /usr/libexec/mknetid

.include <bsd.prog.mk>
