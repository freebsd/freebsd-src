#
# $FreeBSD: src/gnu/usr.bin/perl/utils/perlbug/Makefile,v 1.3 1999/08/27 23:36:28 peter Exp $
#

PROG=	perlbug

.include <bsd.prog.mk>

${PROG}: patchlevel.h

CLEANFILES+=	patchlevel.h

patchlevel.h: ${PERL5SRC}/patchlevel.h
	@ln -sf ${.OODATE} ${.TARGET}
