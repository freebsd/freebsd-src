#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD: src/usr.sbin/route6d/Makefile,v 1.2.2.2 2001/04/25 12:10:44 ru Exp $

PROG=	route6d
MAN=	route6d.8

CFLAGS+=	-Dss_len=__ss_len -Dss_family=__ss_family -DINET6 \
	-DHAVE_GETIFADDRS

.include <bsd.prog.mk>
