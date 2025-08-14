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
SRCS+=	fdopendir.c opendir.c opendir2.c closedir.c readdir.c telldir.c

# Avoid further dependencies by providing simple implementations of libc
# functions such as __error(), etc.
.PATH: ${SRCTOP}/libexec/rtld-elf/rtld-libc
SRCS+=	rtld_libc.c

# Now build the remaining files from libc:
.PATH: ${LIBC_SRCTOP}/stdlib
SRCS+=	reallocf.c realpath.c merge.c reallocarray.c
# TODO: fix merge.c to build with WARNS=6
.if ${COMPILER_TYPE} == "clang"
CFLAGS.merge.c+=-Wno-error=null-pointer-arithmetic
.endif
.PATH: ${LIBC_SRCTOP}/gen
SRCS+=	errlst.c getcwd.c getprogname.c raise.c sigsetops.c sysctlnametomib.c \
    __xuname.c
# errlst.c needs the errlst.h header from libc:
CFLAGS.errlst.c+=-I${LIBC_SRCTOP}/include

# use generic versions of string functions to avoid potential ifunc dispatch
.PATH: ${LIBC_SRCTOP}/string
SRCS+=	bcopy.c bzero.c memchr.c memcmp.c memcpy.c memmove.c memset.c strcat.c \
    strchr.c strchrnul.c strcmp.c strcpy.c strcspn.c strdup.c strlcat.c \
    strlcpy.c strlen.c strncmp.c strncpy.c strrchr.c strsep.c strspn.c \
    strstr.c strtok.c
CFLAGS.memchr.c+=-Wno-cast-qual
CFLAGS.strchr.c+=-Wno-cast-qual
CFLAGS.strchrnul.c+=-Wno-cast-qual
CFLAGS.strcspn.c+=-Wno-sign-compare
CFLAGS.strrchr.c+=-Wno-cast-qual
CFLAGS.strspn.c+=-Wno-sign-compare
CFLAGS.strstr.c+=-Wno-cast-qual -Wno-sign-compare
CFLAGS.strtok.c+=-Wno-cast-qual

# Also use all the syscall .o files from libsys_pic (libsys is always NO_SSP):
_libsys_other_objects= fstat fstatat fstatfs syscall \
    cerror geteuid getegid sigfastblock munmap mprotect \
    sysarch __sysctl issetugid __getcwd utrace getpid \
    thr_self thr_kill pread mmap lseek _exit \
    getdirentries _close _fcntl _open _openat _read \
    _sigprocmask _write readlink ___realpathat
# A few other bits from libc_nossp_pic:
_libc_other_objects= sigsetjmp lstat stat _setjmp setjmp setjmperr

# Finally add additional architecture-dependent libc and libsys dependencies
.if ${LIBC_ARCH} == "arm"
# ARM needs aeabi_unwind_cpp for _setjmp
_libc_other_objects+=aeabi_unwind_cpp
.elif ${LIBC_ARCH} == "i386"
# i386 needs i386_set_gsbase for allocate_initial_tls()
_libsys_other_objects+=i386_set_gsbase
.elif ${LIBC_ARCH} == "powerpc" || ${LIBC_ARCH} == "powerpcspe"
# ppc needs __syncicache and abs for reloc.c
_libc_other_objects+=syncicache abs
.elif ${LIBC_ARCH} == "powerpc64"
# ppc64 needs __syncicache for reloc.c
_libc_other_objects+=syncicache
.endif

# Extract all the .o files from libc_nossp_pic.a and libsys_pic.a. This
# ensures that we don't accidentally pull in the interposing table or
# similar by linking directly against libc_nossp_pic.a
_rtld_libc_objs=
.for _obj in ${_libc_other_objects}
_rtld_libc_objs+=${_obj}.nossppico
CLEANFILES+=${_obj}.nossppico
# LDFLAGS+=	-Wl,--trace-symbol=${_obj}
.endfor
_rtld_libsys_objs=
.for _obj in ${_libsys_other_objects}
_rtld_libsys_objs+=${_obj}.pico
CLEANFILES+=${_obj}.pico
# LDFLAGS+=	-Wl,--trace-symbol=${_obj}
.endfor
# LDFLAGS+=	-Wl,--trace

# We insert all the .o files from libc_nossp_pic.a into a new rtld_libc.a file
# to ensure that only .o files that are actually used end up being included.
#
# XXX: we add dependencies on the source libraries in ../Makefile after
# bsd.prog.mk includes bsd.libnames.mk since variables in dependencies are
# expanded when parsed.
rtld_libc.a: ${SRCTOP}/libexec/rtld-elf/rtld-libc/Makefile.inc
	@rm -f ${.TARGET}
	${AR} x ${LIBC_NOSSP_PIC} ${_rtld_libc_objs}
	${AR} x ${LIBSYS_PIC} ${_rtld_libsys_objs}
	${AR} cr ${.TARGET} ${_rtld_libc_objs} ${_rtld_libsys_objs}
CLEANFILES+=rtld_libc.a
LDADD+=${.OBJDIR}/rtld_libc.a
beforelinking: rtld_libc.a
