#
# $Id$
#

PROG=	a2p
CFLAGS+=-I${PERL5SRC}/x2p -I${.OBJDIR} -I${.OBJDIR}/temp
SRCS=	a2p.c hash.c str.c util.c walk.c

beforedepend all: ${.OBJDIR}/config.h temp

.include <bsd.prog.mk>

.PATH: ${PERL5SRC}/x2p

.SUFFIXES:
.SUFFIXES: .c .h .o

temp:
	@mkdir temp

CLEANFILES+= ${.OBJDIR}/config.h ${.OBJDIR}/config_h.sh ${.OBJDIR}/config.sh \
		${PROG}.1

${.OBJDIR}/config.h: ${.OBJDIR}/config_h.sh ${.OBJDIR}/config.sh
	sh config_h.sh

${.OBJDIR}/config.sh: ${.CURDIR}/../../libperl/config.SH
	@ln -sf ${.OODATE} ${.TARGET}

${.OBJDIR}/config_h.sh: ${PERL5SRC}/config_h.SH
	@ln -sf ${.OODATE} ${.TARGET}

${PROG}.1: ${PROG}.pod
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}
