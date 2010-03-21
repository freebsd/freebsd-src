# $FreeBSD: src/share/doc/papers/timecounter/Makefile,v 1.3.24.1 2010/02/10 00:26:20 kensmith Exp $

# You really want:
# PRINTERDEVICE=ps
# or you will not get the illustration.
VOLUME=	papers
DOC=	timecounter
SRCS=	tmac.usenix timecounter.ms-patched
EXTRA=	fig1.eps fig2.eps fig3.eps fig4.eps fig5.eps gps.ps intr.ps
MACROS=	-ms
CLEANFILES=	timecounter.ms-patched
USE_PIC=
USE_EQN=
USE_TBL=

timecounter.ms-patched: timecounter.ms
	sed -E -e 's;(gps|intr).ps;${.CURDIR}/&;' -e 's;fig[0-9].eps;${.CURDIR}/&;' \
		${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
