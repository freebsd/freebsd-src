#
# $FreeBSD$
#

LIB=	perl

CFLAGS+=-I${.OBJDIR} -I${PERL5SRC}

SRCS=	perl.c gv.c toke.c perly.c op.c regcomp.c dump.c util.c mg.c byterun.c \
	hv.c av.c run.c pp_hot.c sv.c pp.c scope.c pp_ctl.c pp_sys.c \
	doop.c doio.c regexec.c taint.c deb.c universal.c globals.c perlio.c \
	config.h

LDADD=	-lm

NO_PERL_SCRIPT_MAKE=	true

# NOPIC=	true

.include <bsd.lib.mk>

build-tools:	cleandepend
	cd ${.CURDIR} && ${MAKE} lib${LIB}.a

config.h:	links
	@sh config_h.sh

.SUFFIXES:

.SUFFIXES: .o .po .So .s .S .c

.PATH:	${PERL5SRC}
