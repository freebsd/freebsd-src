#
# $FreeBSD: src/gnu/usr.bin/perl/utils/perlbug/Makefile,v 1.2.2.1 1999/08/29 14:34:30 peter Exp $
#

PROG=	perlbug

.include <bsd.prog.mk>

${PROG}: patchlevel.h

CLEANFILES+=	patchlevel.h

patchlevel.h: ${PERL5SRC}/patchlevel.h
	@ln -sf ${.OODATE} ${.TARGET}
