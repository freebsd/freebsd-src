#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/rmt/Makefile,v 1.10.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	rmt
MAN=	rmt.8

WARNS?=	4

# called from /usr/src/etc/Makefile
etc-rmt:
	rm -f ${DESTDIR}/etc/rmt
	ln -s ${BINDIR}/rmt ${DESTDIR}/etc/rmt

.include <bsd.prog.mk>
