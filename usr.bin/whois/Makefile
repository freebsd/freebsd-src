#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/whois/Makefile,v 1.7.28.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	whois

.if defined(SOCKS)
CFLAGS+=-DSOCKS
CFLAGS+=-Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dlisten=Rlisten \
	-Daccept=Raccept -Drcmd=Rrcmd -Dbind=Rbind -Dselect=Rselect
LDADD+=	-lsocks
.endif

.include <bsd.prog.mk>
