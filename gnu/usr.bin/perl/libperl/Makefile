#
# $Id$
#

LIB=	perl

CFLAGS+=-I${.OBJDIR} -I${PERL5SRC}

SRCS=	perl.c gv.c toke.c perly.c op.c regcomp.c dump.c util.c mg.c byterun.c \
	hv.c av.c run.c pp_hot.c sv.c pp.c scope.c pp_ctl.c pp_sys.c \
	doop.c doio.c regexec.c taint.c deb.c universal.c globals.c perlio.c \
	config.h

NOPIC=	true

beforeinstall beforedepend all: config.h config.sh

CLEANFILES+=	config.h config.sh config_h.sh

.include <bsd.lib.mk>

.SUFFIXES:

.SUFFIXES: .a .c .h .o .po .sh

.PATH:	${PERL5SRC}

config.h: config_h.sh config.sh
	sh config_h.sh

config.sh: ${.CURDIR}/config.SH
	ln -sf ${.OODATE} ${.TARGET}

config_h.sh: config_h.SH
	ln -sf ${.OODATE} ${.TARGET}

test:
	@echo ${PERL5SRC}
