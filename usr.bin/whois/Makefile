#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAINTAINER=	mike@FreeBSD.org

PROG=	whois
WARNS?=	2

.if defined(SOCKS)
CFLAGS+=-DSOCKS
CFLAGS+=-Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dlisten=Rlisten \
	-Daccept=Raccept -Drcmd=Rrcmd -Dbind=Rbind -Dselect=Rselect
LDADD+=	-lsocks
.endif

.include <bsd.prog.mk>
