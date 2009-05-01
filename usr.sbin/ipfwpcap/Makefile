#
# From: Id: Makefile,v 1.2 2004/01/15 16:20:56 pkern Exp
#
# $FreeBSD: src/usr.sbin/ipfwpcap/Makefile,v 1.2.10.1 2009/04/15 03:14:26 kensmith Exp $
#

PROG=	ipfwpcap

LDADD=	-lpcap
DPADD=	${LIBPCAP}

MAN=	ipfwpcap.8

.include <bsd.prog.mk>

test:	$(CMD)
	-rm /var/run/ipfwpcap.2000.pid
	./ipfwpcap -d 2000 - | tcpdump -r - -n -s 2000 -X

