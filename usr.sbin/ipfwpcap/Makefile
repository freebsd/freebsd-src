#
# From: Id: Makefile,v 1.2 2004/01/15 16:20:56 pkern Exp
#
# $FreeBSD: src/usr.sbin/ipfwpcap/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#

PROG=	ipfwpcap

LDADD=	-lpcap
DPADD=	${LIBPCAP}

MAN=	ipfwpcap.8

WARNS?=	6

.include <bsd.prog.mk>

test:	$(CMD)
	-rm /var/run/ipfwpcap.2000.pid
	./ipfwpcap -d 2000 - | tcpdump -r - -n -s 2000 -X

