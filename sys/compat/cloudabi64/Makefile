# $FreeBSD$

.include <src.lua.mk>

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

all:
	@echo "make sysent only"

sysent: cloudabi64_sysent.c cloudabi64_syscall.h cloudabi64_proto.h \
    cloudabi64_syscalls.c cloudabi64_systrace_args.c

cloudabi64_sysent.c cloudabi64_syscall.h cloudabi64_proto.h \
    cloudabi64_syscalls.c cloudabi64_systrace_args.c: \
    ../../tools/makesyscalls.lua ../../contrib/cloudabi/syscalls64.master \
    ${.CURDIR}/syscalls.conf
	${LUA} ../../tools/makesyscalls.lua \
	    ../../contrib/cloudabi/syscalls64.master ${.CURDIR}/syscalls.conf
