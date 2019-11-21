# $FreeBSD$

.include <src.opts.mk>

PROG=	jail
MAN=	jail.8 jail.conf.5
SRCS=	jail.c command.c config.c state.c jailp.h jaillex.l jailparse.y y.tab.h

LIBADD=	jail kvm util

PACKAGE=jail

NO_WMISSING_VARIABLE_DECLARATIONS=

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

# workaround for GNU ld (GNU Binutils) 2.33.1:
#   relocation truncated to fit: R_RISCV_GPREL_I against `.LANCHOR2'
# https://bugs.freebsd.org/242109
.if defined(LINKER_TYPE) && ${LINKER_TYPE} == "bfd" && ${MACHINE} == "riscv"
CFLAGS+=-Wl,--no-relax
.endif

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
