#
# $FreeBSD$
#

PROG=	a2p
CFLAGS+=-I${PERL5SRC}/x2p -I${.OBJDIR} -I${.OBJDIR}/temp
LDFLAGS+=-lperl -lm -lcrypt
SRCS=	a2p.c hash.c str.c util.c walk.c config.h

beforedepend all: config.h temp

.include <bsd.prog.mk>

PERL_THREADED?= no
.if ${PERL_THREADED} == "true"
CFLAGS+=        -pthread
LDADD+=         -pthread
.endif
     
PERL_DEBUGGING?=        no
.if ${PERL_DEBUGGING} == "true"
CFLAGS+=        -DDEBUGGING
.endif

.PATH: ${PERL5SRC}/x2p
.PATH: ${PERL5SRC}

.SUFFIXES:
.SUFFIXES: .c .h .o

temp:
	@mkdir temp

CLEANFILES+= config.h config_h.sh config.sh ${PROG}.1

config.h: config_h.sh config.sh
	sh config_h.sh

${.CURDIR}/../../libperl/config.sh:
	cd ${.CURDIR}/../../libperl ; make config.sh

config.sh: ${.CURDIR}/../../libperl/config.sh
	ln -sf  ${CANONICALOBJDIR}/../../libperl/config.sh ${.TARGET}

config_h.sh: ${PERL5SRC}/config_h.SH
	ln -sf ${.OODATE} ${.TARGET}

${PROG}.1: ${PROG}.pod
	miniperl ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}
