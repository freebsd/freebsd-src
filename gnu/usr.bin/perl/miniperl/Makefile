# $FreeBSD$

PROG=	miniperl
NOMAN=	true
CFLAGS+=-I${PERL5SRC} -I${.OBJDIR} -DPERL_EXTERNAL_GLOB
SRCS=	miniperlmain.c opmini.c config.h
# Miniperl _must_ be static!!
NOSHARED= yes
.if exists(${.OBJDIR}/../libperl/)
MYLIBPERL= ${.OBJDIR}/../libperl/libperl.a
.else
MYLIBPERL= ${.CURDIR}/../libperl/libperl.a
.endif
DPADD=	${MYLIBPERL} ${LIBM} ${LIBCRYPT} ${LIBUTIL}
LDADD=	${MYLIBPERL} -lm -lcrypt -lutil

build-tools:	cleandepend
	cd ${.CURDIR} && ${MAKE} ${PROG}

install distribute:

NO_PERL_SCRIPT_MAKE=	true

.include <bsd.prog.mk>

.PATH:	${PERL5SRC}

opmini.c:	op.c
	ln -sf ${.OODATE} ${.TARGET}

config.h:	links
	@sh config_h.sh

CLEANFILES+=	opmini.c config.h
