#
# $Id: Makefile,v 1.4 1999/05/02 15:29:56 markm Exp $
#

PROG=	h2ph

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	miniperl ${.OBJDIR}/${PROG} -d \
		${DESTDIR}/usr/libdata/perl/5.00503/mach * */*
