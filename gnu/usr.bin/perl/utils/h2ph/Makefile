#
# $Id: Makefile,v 1.1 1998/09/09 07:20:29 markm Exp $
#

PROG=	h2ph

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	miniperl ${.OBJDIR}/${PROG} -d \
		${DESTDIR}/usr/libdata/perl/5.00502/mach * sys/*
