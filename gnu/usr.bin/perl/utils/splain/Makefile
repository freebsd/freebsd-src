#
# $Id: Makefile,v 1.1 1998/09/09 07:20:32 markm Exp $
#

PROG=	splain
NOMAN=	true

.include <bsd.prog.mk>

${PROG}: ${.OBJDIR}/diagnostics.pm

CLEANFILES+=	${.OBJDIR}/diagnostics.pm

${.OBJDIR}/diagnostics.pm: ${.OBJDIR}/../../perl/lib/diagnostics.pm
	@ln -sf ${.OODATE} ${.TARGET}
