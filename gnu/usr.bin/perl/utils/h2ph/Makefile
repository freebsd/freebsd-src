#
# $FreeBSD$
#

SCRIPTS=h2ph

beforedepend all:	scripts

.include <bsd.prog.mk>

afterinstall:
	cd ${DESTDIR}/usr/include; \
	${MINIPERL} -I${.OBJDIR}/lib ${.OBJDIR}/${SCRIPTS} -d \
		${DESTDIR}/usr/libdata/perl/${VERSION}/mach * */*

.PATH:	${PERL5SRC}
