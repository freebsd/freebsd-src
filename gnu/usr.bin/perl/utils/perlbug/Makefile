#
# $Id: Makefile,v 1.2 1998/09/22 12:00:58 markm Exp $
#

PROG=	perlbug

.include <bsd.prog.mk>

${PROG}: patchlevel.h

CLEANFILES+=	patchlevel.h

patchlevel.h: ${PERL5SRC}/patchlevel.h
	@ln -sf ${.OODATE} ${.TARGET}
