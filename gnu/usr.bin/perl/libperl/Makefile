#
# $FreeBSD: src/gnu/usr.bin/perl/libperl/Makefile,v 1.9.2.3 2002/02/19 16:21:34 ru Exp $
#

LIB=	perl

CFLAGS+=-I${.OBJDIR} -I${PERL5SRC}

SRCS=	perl.c gv.c toke.c perly.c op.c regcomp.c dump.c util.c mg.c byterun.c \
	hv.c av.c run.c pp_hot.c sv.c pp.c scope.c pp_ctl.c pp_sys.c \
	doop.c doio.c regexec.c taint.c deb.c universal.c globals.c perlio.c \
	config.h

DPADD=	${LIBM} ${LIBUTIL}
LDADD=	-lm -lutil

NO_PERL_SCRIPT_MAKE=	true

# NOPIC=	true

.include <bsd.lib.mk>

config.h:	links
	@sh config_h.sh

.SUFFIXES:

.SUFFIXES: .o .po .So .s .S .c

.PATH:	${PERL5SRC}
