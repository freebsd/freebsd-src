#
# $FreeBSD$
#

LIB=	perl

CFLAGS+=-I${.OBJDIR} -I${PERL5SRC}

SRCS=	perl.c gv.c toke.c perly.c op.c regcomp.c dump.c util.c mg.c byterun.c \
	hv.c av.c run.c pp_hot.c sv.c pp.c scope.c pp_ctl.c pp_sys.c \
	doop.c doio.c regexec.c taint.c deb.c universal.c globals.c perlio.c \
	config.h

# NOPIC=	true

beforeinstall beforedepend all: config.h config.sh

CLEANFILES+=	config.h config.sh config_h.sh

.include <bsd.lib.mk>

PERL_THREADED?= no
.if ${PERL_THREADED} == "true"
CFLAGS+=        -pthread
.endif

PERL_DEBUGGING?=        no
.if ${PERL_DEBUGGING} == "true"
CFLAGS+=        -DDEBUGGING
.endif

.SUFFIXES:

.SUFFIXES: .o .po .So .s .S .c

.PATH:	${PERL5SRC}

config.h: config_h.sh config.sh
	sh config_h.sh

.if ${PERL_THREADED} == "true"
config.sh: config.SH-threads-${OBJFORMAT}.${MACHINE_ARCH}
	ln -sf ${.OODATE} ${.TARGET}
.else
config.sh: config.SH-${OBJFORMAT}.${MACHINE_ARCH}
	ln -sf ${.OODATE} ${.TARGET}
.endif

config_h.sh: config_h.SH
	ln -sf ${.OODATE} ${.TARGET}
