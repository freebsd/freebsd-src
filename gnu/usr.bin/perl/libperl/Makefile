# $FreeBSD$

LIB=	perl

CFLAGS+=-I${.OBJDIR} -I${PERL5SRC} -DPERL_CORE

SRCS=	perl.c gv.c toke.c perly.c op.c regcomp.c dump.c util.c mg.c \
	hv.c av.c run.c pp_hot.c sv.c pp.c scope.c pp_ctl.c pp_sys.c \
	doop.c doio.c regexec.c utf8.c taint.c deb.c universal.c \
	xsutils.c globals.c perlio.c perlapi.c \
	config.h

DPADD=	${LIBM}
LDADD=	-lm

NO_PERL_SCRIPT_MAKE=	true

# NOPIC=	true

.include <bsd.lib.mk>

config.h:	links
	@sh config_h.sh

.SUFFIXES:

.SUFFIXES: .o .po .So .s .S .c .ln

.PATH:	${PERL5SRC}
