#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/isdn/Makefile,v 1.4.36.1 2010/02/10 00:26:20 kensmith Exp $

FILES=	0.al 1.al 2.al 3.al 4.al 5.al 6.al 7.al 8.al 9.al beep.al msg.al

CLEANFILES+=	${FILES}

.SUFFIXES: .uu

.uu:
	uudecode < ${.IMPSRC}

FILESDIR=	${BINDIR}/isdn

.include <bsd.prog.mk>
