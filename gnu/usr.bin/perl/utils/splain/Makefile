#
# $FreeBSD: src/gnu/usr.bin/perl/utils/splain/Makefile,v 1.1.2.1 1999/08/29 14:34:35 peter Exp $
#

PROG=	splain
NOMAN=	true

.include <bsd.prog.mk>

${PROG}: ${.OBJDIR}/diagnostics.pm

CLEANFILES+=	${.OBJDIR}/diagnostics.pm

${.OBJDIR}/diagnostics.pm: ${.OBJDIR}/../../perl/lib/diagnostics.pm
	@ln -sf ${.OODATE} ${.TARGET}
