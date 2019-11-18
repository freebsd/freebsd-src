# $FreeBSD$

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

.include <src.lua.mk>

all:
	@echo "make sysent only"

sysent: cloudabi32_sysent.c cloudabi32_syscall.h cloudabi32_proto.h \
    cloudabi32_syscalls.c cloudabi32_systrace_args.c

cloudabi32_sysent.c cloudabi32_syscall.h cloudabi32_proto.h \
    cloudabi32_syscalls.c cloudabi32_systrace_args.c: \
    ../../tools/makesyscalls.lua ../../contrib/cloudabi/syscalls32.master \
    ${.CURDIR}/syscalls.conf
	${LUA} ../../tools/makesyscalls.lua \
	    ../../contrib/cloudabi/syscalls32.master ${.CURDIR}/syscalls.conf
