#
# $FreeBSD$
#

PROG=	perlbug

.include <bsd.prog.mk>

${PROG}: patchlevel.h

CLEANFILES+=	patchlevel.h

patchlevel.h: ${PERL5SRC}/patchlevel.h
	@ln -sf ${.OODATE} ${.TARGET}
