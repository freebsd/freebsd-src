
# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <bsd.sysdir.mk>
.include <src.lua.mk>

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

MAKESYSCALLS_INTERP?=	${LUA}
MAKESYSCALLS_SCRIPT?=	${SYSDIR}/tools/syscalls/main.lua
MAKESYSCALLS=	${MAKESYSCALLS_INTERP} ${MAKESYSCALLS_SCRIPT}

all:
	@echo "make sysent only"

# We .ORDER these explicitly so that we only run MAKESYSCALLS once, rather than
# potentially once for each ${GENERATED} file.
.ORDER: ${GENERATED}
sysent: ${GENERATED}

# We slap a .PHONY on MAKESYSCALLS_SCRIPT so that we regenerate every
# single time rather than tracking all internal dependencies for now.
${MAKESYSCALLS_SCRIPT}: .PHONY

${GENERATED}: ${MAKESYSCALLS_SCRIPT} ${SRCS}
	${MAKESYSCALLS} ${SYSENT_FILE} ${SYSENT_CONF}
