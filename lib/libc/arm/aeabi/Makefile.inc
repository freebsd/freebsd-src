# $FreeBSD$

.PATH: ${.CURDIR}/arm/aeabi

SRCS+=	aeabi_atexit.c		\
	aeabi_double.c		\
	aeabi_float.c		\
	aeabi_unwind_cpp.c

# Add the aeabi_mem* functions. While they live in compiler-rt they call into
# libc. This causes issues when other parts of libc call these functions.
# We work around this by including these functions in libc but mark them as
# hidden so users of libc will not pick up these versions.
.PATH: ${.CURDIR}/../../contrib/compiler-rt/lib/arm

SRCS+=	aeabi_memcmp.S		\
	aeabi_memcpy.S		\
	aeabi_memmove.S		\
	aeabi_memset.S

# Mark the functions as hidden so they are not available outside of libc.
CFLAGS.aeabi_memcmp.S=	-DVISIBILITY_HIDDEN
CFLAGS.aeabi_memcpy.S=	-DVISIBILITY_HIDDEN
CFLAGS.aeabi_memmove.S=	-DVISIBILITY_HIDDEN
CFLAGS.aeabi_memset.S=	-DVISIBILITY_HIDDEN
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}


SYM_MAPS+=${.CURDIR}/arm/aeabi/Symbol.map

