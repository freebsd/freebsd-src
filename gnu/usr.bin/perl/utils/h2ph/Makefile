#
# $Id: Makefile,v 1.2.2.2 1999/05/05 13:28:53 markm Exp $
#

PROG=	h2ph

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	miniperl ${.OBJDIR}/${PROG} -d \
		${DESTDIR}/usr/libdata/perl/5.00503/mach * */*
