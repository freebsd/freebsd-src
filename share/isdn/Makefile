#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/isdn/Makefile,v 1.3.2.1 2002/02/18 15:03:33 ru Exp $

FILES=	0.al 1.al 2.al 3.al 4.al 5.al 6.al 7.al 8.al 9.al beep.al msg.al

CLEANFILES+=	${FILES}

.SUFFIXES: .uu

.uu:
	uudecode < ${.IMPSRC}

FILESDIR=	${BINDIR}/isdn

.include <bsd.prog.mk>
