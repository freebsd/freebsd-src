#
# $Id$
#

PROG=	perlbug

.include <bsd.prog.mk>

${PROG}: ${.OBJDIR}/patchlevel.h

CLEANFILES+=	${.OBJDIR}/patchlevel.h

${.OBJDIR}/patchlevel.h: ${PERL5SRC}/patchlevel.h
	@ln -sf ${.OODATE} ${.TARGET}
