#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/rmt/Makefile,v 1.9 2002/07/11 21:26:41 alfred Exp $

PROG=	rmt
WARNS?=	4
MAN=	rmt.8

# called from /usr/src/etc/Makefile
etc-rmt:
	rm -f ${DESTDIR}/etc/rmt
	ln -s ${BINDIR}/rmt ${DESTDIR}/etc/rmt

.include <bsd.prog.mk>
