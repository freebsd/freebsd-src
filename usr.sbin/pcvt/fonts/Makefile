# $FreeBSD: src/usr.sbin/pcvt/fonts/Makefile,v 1.10.2.1 2002/02/18 16:24:27 ru Exp $

.SUFFIXES: .uu
.uu:
	rm -f ${.TARGET}
	uudecode ${.IMPSRC}

FILES=	vt220h.808 vt220h.810 vt220h.814 vt220h.816 \
	vt220l.808 vt220l.810 vt220l.814 vt220l.816
#FILES+=	vt100pc.814 vt100sg.814
FILESDIR=	${FONTDIR}
CLEANFILES= 	${FILES}

.include <bsd.prog.mk>
