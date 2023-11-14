.PATH:	${LIBC_SRCTOP}/x86/sys
.PATH:	${LIBSYS_SRCTOP}/x86

SRCS+= \
	__vdso_gettc.c \
	pkru.c \
	sched_getcpu_x86.c

MAN+=	\
	pkru.3

# Note: vdso support for hyperv only on amd64
.if ${MACHINE_CPUARCH} == "amd64" && ${MK_HYPERV} != "no"
CFLAGS+=	-DWANT_HYPERV
.endif
# We can't use sanitizer instrumentation on ifuncs called during sanitizer
# runtime startup.
.if ${MK_ASAN} != "no"
CFLAGS.__vdso_gettc.c+=-fno-sanitize=address
.endif
.if ${MK_UBSAN} != "no"
CFLAGS.__vdso_gettc.c+=-fno-sanitize=undefined
.endif
