#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD: src/usr.sbin/route6d/Makefile,v 1.2 2000/01/07 10:02:40 shin Exp $

PROG=	route6d
MAN8=	route6d.8

CFLAGS+=	-Dss_len=__ss_len -Dss_family=__ss_family -DINET6

.include <bsd.prog.mk>
