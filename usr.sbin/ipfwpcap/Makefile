#
# From: Id: Makefile,v 1.2 2004/01/15 16:20:56 pkern Exp
#
# $FreeBSD$
#

PROG=	ipfwpcap

LDADD=	-lpcap
DPADD=	${LIBPCAP}

NO_MAN=	not_yet

.include <bsd.prog.mk>

test:	$(CMD)
	-rm /var/run/ipfwpcap.2000.pid
	./ipfwpcap -d 2000 - | tcpdump -r - -n -s 2000 -X

