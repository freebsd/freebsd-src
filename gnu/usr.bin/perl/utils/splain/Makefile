#
# $FreeBSD: src/gnu/usr.bin/perl/utils/splain/Makefile,v 1.2 1999/08/27 23:36:31 peter Exp $
#

PROG=	splain
NOMAN=	true

.include <bsd.prog.mk>

${PROG}: ${.OBJDIR}/diagnostics.pm

CLEANFILES+=	${.OBJDIR}/diagnostics.pm

${.OBJDIR}/diagnostics.pm: ${.OBJDIR}/../../perl/lib/diagnostics.pm
	@ln -sf ${.OODATE} ${.TARGET}
