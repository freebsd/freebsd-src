#
# $Id: Makefile,v 1.1 1998/09/09 07:20:30 markm Exp $
#

PROG=	perlbug

.include <bsd.prog.mk>

${PROG}: patchlevel.h

CLEANFILES+=	patchlevel.h

patchlevel.h: ${PERL5SRC}/patchlevel.h
	@ln -sf ${.OODATE} ${.TARGET}
