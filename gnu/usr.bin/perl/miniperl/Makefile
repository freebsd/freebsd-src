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
DPADD=	${MYLIBPERL} ${LIBM} ${LIBCRYPT}
LDADD=	${MYLIBPERL} -lm -lcrypt

build-tools:	${PROG}

.include <bsd.prog.mk>

.PATH:	${PERL5SRC}

opmini.c:	op.c
	ln -sf ${.OODATE} ${.TARGET}

CLEANFILES+=	opmini.c
