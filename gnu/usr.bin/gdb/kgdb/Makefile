# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_PURE} == "yes"
# linker error / uses sbrk
WANT_CHERI=	none
.endif

PROG=	kgdb${GDB_SUFFIX}
SRCS=	main.c kld.c kthr.c trgt.c trgt_${TARGET_CPUARCH}.c
WARNS?=	2

LIBADD+=	gdb bfd opcodes iberty m readline ncursesw gnuregex kvm

.include <bsd.prog.mk>
