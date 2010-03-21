# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $FreeBSD: src/usr.bin/passwd/Makefile,v 1.47.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

PROG	 = passwd
BINOWN	 = root
BINMODE	 = 4555
DPADD	 = ${LIBPAM}
LDADD	 = ${MINUSLPAM}
.if ${MK_NIS} != "no"
LINKS	 = ${BINDIR}/passwd ${BINDIR}/yppasswd
MLINKS	 = passwd.1 yppasswd.1
.endif
WARNS	?= 4

beforeinstall:
.for i in passwd yppasswd
	[ ! -e ${DESTDIR}${BINDIR}/$i ] || \
		chflags noschg ${DESTDIR}${BINDIR}/$i || true
.endfor

afterinstall:
	-chflags schg ${DESTDIR}${BINDIR}/passwd

.include <bsd.prog.mk>
