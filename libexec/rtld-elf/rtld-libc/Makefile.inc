# $FreeBSD$
# This makefiles adds the necessary libc dependencies for RTLD without pulling
# in all of the complex libc bits such as locales, etc.

.include <bsd.compiler.mk>

LIBC_SRCTOP=${SRCTOP}/lib/libc
.if exists(${LIBC_SRCTOP}/${MACHINE_ARCH:S/powerpc64le/powerpc64/})
LIBC_ARCH=${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.else
LIBC_ARCH=${MACHINE_CPUARCH}
.endif

CFLAGS+=	-I${SRCTOP}/libexec/rtld-elf/rtld-libc

# Build all the libc files that use interposed symbols or pthreads again for
# RTLD. We compile with a different libc_private.h and namespace.h that
# redirects all calls to interposed functions to use the non-interposed version
# instead.
.PATH: ${LIBC_SRCTOP}/gen
SRCS+=	opendir.c closedir.c readdir.c telldir.c

# Avoid further dependencies by providing simple implementations of libc
# functions such as __error(), etc.
.PATH: ${SRCTOP}/libexec/rtld-elf/rtld-libc
SRCS+=	rtld_libc.c

# Now build the remaining files from libc:
.PATH: ${LIBC_SRCTOP}/stdlib
SRCS+=	reallocf.c realpath.c getenv.c merge.c reallocarray.c
# TODO: fix merge.c to build with WARNS=6
.if ${COMPILER_TYPE} == "clang"
CFLAGS.merge.c+=-Wno-error=null-pointer-arithmetic
.endif
.PATH: ${LIBC_SRCTOP}/gen
SRCS+=	errlst.c getcwd.c getprogname.c raise.c sigsetops.c sysctlnametomib.c \
    __xuname.c
# errlst.c needs the errlst.h header from libc:
CFLAGS.errlst.c+=-I${LIBC_SRCTOP}/include

# Use the string and memory .o files from libc instead of rebuilding them (they
# might be using optimized assembly and duplicating that logic here is awkward).
_libc_string_objects=	bcmp bcopy bzero memset memchr memcmp memcpy memmove \
    stpncpy strcat strchr strcmp stpcpy strcpy strcspn strdup strlcat strlcpy \
    strlen strncmp strncpy strrchr strsep strspn strstr strtok
# Also use all the syscall .o files from libc_nossp_pic:
_libc_other_objects= sigsetjmp lstat stat fstat fstatat fstatfs syscall \
    cerror geteuid getegid sigfastblock munmap mprotect \
    sysarch __sysctl issetugid __getcwd utrace getpid \
    thr_self thr_kill pread mmap lseek _exit _fstat _fstatat _fstatfs \
    getdirentries _getdirentries _close _fcntl _open _openat _read \
    _sigprocmask _write readlink __realpathat _setjmp setjmp setjmperr


# Finally add additional architecture-dependent libc dependencies
.if ${LIBC_ARCH} == "arm"
# ARM needs aeabi_unwind_cpp for _setjmp
_libc_other_objects+=aeabi_unwind_cpp
.elif ${LIBC_ARCH} == "i386"
# __udivdi3 is needed by kvprintf() in rtld_printf.c
# i386 also needs i386_set_gsbase for allocate_initial_tls()
_libc_other_objects+=umoddi3 udivdi3 qdivrem i386_set_gsbase
.elif ${LIBC_ARCH} == "powerpc" || ${LIBC_ARCH} == "powerpcspe"
# ppc needs __syncicache for reloc.c and __umoddi3+__udivdi3 for rtld_printf.c
_libc_other_objects+=syncicache umoddi3 udivdi3 qdivrem
# for some reason there is also a reference to abs()
_libc_other_objects+=abs
.elif ${LIBC_ARCH} == "powerpc64"
# ppc64 needs __syncicache for reloc.c
_libc_other_objects+=syncicache
.elif ${LIBC_ARCH} == "mips"
# 32-bit MIPS needs __umoddi3+__udivdi3 for rtld_printf.c
.if ${MACHINE_ARCH:Mmipsn32*} == "" && ${MACHINE_ARCH:Mmips64*} == ""
_libc_other_objects+=umoddi3 udivdi3 qdivrem
.endif
.endif

# Extract all the .o files from libc_nossp_pic.a. This ensures that
# we don't accidentally pull in the interposing table or similar by linking
# directly against libc_nossp_pic.a
_rtld_libc_objs=
.for _obj in ${_libc_other_objects} ${_libc_string_objects}
_rtld_libc_objs+=${_obj}.nossppico
CLEANFILES+=${_obj}.nossppico
# LDFLAGS+=	-Wl,--trace-symbol=${_obj}
.endfor
# LDFLAGS+=	-Wl,--trace

# We insert all the .o files from libc_nossp_pic.a into a new rtld_libc.a file
# to ensure that only .o files that are actually used end up being included.
rtld_libc.a: ${LIBC_NOSSP_PIC} ${SRCTOP}/libexec/rtld-elf/rtld-libc/Makefile.inc
	${AR} x ${LIBC_NOSSP_PIC} ${_rtld_libc_objs}
	${AR} cr ${.OBJDIR}/${.TARGET} ${_rtld_libc_objs}
CLEANFILES+=rtld_libc.a
LDADD+=${.OBJDIR}/rtld_libc.a
beforelinking: rtld_libc.a
