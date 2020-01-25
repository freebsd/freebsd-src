# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <bsd.sysdir.mk>

COMMON_GENERATED=	proto.h		\
			syscall.h	\
			syscalls.c	\
			sysent.c	\
			systrace_args.c

GENERATED_PREFIX?=
GENERATED?=	${COMMON_GENERATED:S/^/${GENERATED_PREFIX}/}
SYSENT_FILE?=	syscalls.master
SYSENT_CONF?=	syscalls.conf

# Including Makefile should override SYSENT_FILE and SYSENT_CONF as needed,
# and set GENERATED.
SRCS+=	${SYSENT_FILE}
SRCS+=	${SYSENT_CONF}

MAKESYSCALLS_INTERP?=	sh
MAKESYSCALLS_SCRIPT?=	${SYSDIR}/kern/makesyscalls.sh
MAKESYSCALLS=	${MAKESYSCALLS_INTERP} ${MAKESYSCALLS_SCRIPT}

all:
	@echo "make sysent only"

# We .ORDER these explicitly so that we only run MAKESYSCALLS once, rather than
# potentially once for each ${GENERATED} file.
.ORDER: ${GENERATED}
sysent: ${GENERATED}

${GENERATED}: ${MAKESYSCALLS_SCRIPT} ${SRCS}
	${MAKESYSCALLS} ${SYSENT_FILE} ${SYSENT_CONF}
