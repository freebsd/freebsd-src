#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/whois/Makefile,v 1.7.30.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	whois

.if defined(SOCKS)
CFLAGS+=-DSOCKS
CFLAGS+=-Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dlisten=Rlisten \
	-Daccept=Raccept -Drcmd=Rrcmd -Dbind=Rbind -Dselect=Rselect
LDADD+=	-lsocks
.endif

.include <bsd.prog.mk>
