#
# $FreeBSD: src/gnu/usr.bin/perl/utils/h2ph/Makefile,v 1.5.2.1 2000/10/05 09:54:53 markm Exp $
#

PROG=	h2ph

beforedepend all:	scripts

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	${MINIPERL} -I${.OBJDIR}/lib ${.OBJDIR}/${PROG} -d \
		${DESTDIR}/usr/libdata/perl/${VERSION}/mach * */*

.PATH:	${PERL5SRC}
