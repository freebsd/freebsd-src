#
# $Id$
#

PROG=	splain
NOMAN=	true

.include <bsd.prog.mk>

${PROG}: ${.OBJDIR}/diagnostics.pm

CLEANFILES+=	${.OBJDIR}/diagnostics.pm

${.OBJDIR}/diagnostics.pm: ${.OBJDIR}/../../perl/lib/diagnostics.pm
	@ln -sf ${.OODATE} ${.TARGET}
