#
# $Id: Makefile,v 1.3 1999/03/18 19:39:07 julian Exp $
#

PROG=	h2ph

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	miniperl ${.OBJDIR}/${PROG} -d \
		${DESTDIR}/usr/libdata/perl/5.00503/mach * */*
