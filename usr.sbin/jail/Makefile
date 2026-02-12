.include <src.opts.mk>

PROG=	jail
MAN=	jail.8 jail.conf.5
SRCS=	jail.c command.c config.c state.c jailp.h jaillex.l jailparse.y y.tab.h

LIBADD=	jail kvm util

PACKAGE=jail

NO_WMISSING_VARIABLE_DECLARATIONS=
CWARNFLAGS.jailparse.c=	${NO_WUNUSED_BUT_SET_VARIABLE}

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif
.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif

CLEANFILES= y.output

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
