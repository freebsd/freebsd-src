#
# $FreeBSD$
#

PROG=	miniperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR}
SRCS=	miniperlmain.c config.h
# Miniperl _must_ be static!!
NOSHARED= yes
DPADD=	${LIBPERL} ${LIBM} ${LIBCRYPT}
LDADD=	-lperl -lm -lcrypt

# Trick the bootstrap tools into thinking that miniperl is perl.
# This gets overwritten.
LINKS=	${BINDIR}/${PROG} ${BINDIR}/perl

CLEANFILES=	config.h config.sh config_h.sh

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

miniperlmain.o: config.h

config.h: config_h.sh config.sh
	sh config_h.sh

${.CURDIR}/../libperl/config.sh:
	cd ${.CURDIR}/../libperl ; make config.sh

config.sh: ${.CURDIR}/../libperl/config.sh
	ln -sf ${CANONICALOBJDIR}/../libperl/config.sh ${.TARGET}

config_h.sh: config_h.SH
	ln -sf ${.OODATE} ${.TARGET}

.PATH:	${PERL5SRC}
