#
# $FreeBSD: src/gnu/usr.bin/perl/utils/h2ph/Makefile,v 1.5 1999/08/27 23:36:26 peter Exp $
#

PROG=	h2ph

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	miniperl ${.OBJDIR}/${PROG} -d \
		${DESTDIR}/usr/libdata/perl/5.00503/mach * */*
