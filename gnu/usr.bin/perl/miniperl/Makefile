#
# $FreeBSD: src/gnu/usr.bin/perl/miniperl/Makefile,v 1.7.2.1 1999/08/29 14:34:13 peter Exp $
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

miniperlmain.o: config.h

config.h: config_h.sh config.sh
	sh config_h.sh

config.sh: ${.CURDIR}/../libperl/config.SH-${OBJFORMAT}.${MACHINE_ARCH}
	@ln -sf ${.OODATE} ${.TARGET}

config_h.sh: config_h.SH
	@ln -sf ${.OODATE} ${.TARGET}

.PATH:	${PERL5SRC}
