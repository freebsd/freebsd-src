PACKAGE=	clibs
SHLIBDIR?=	/lib
SHLIB=		ssp
SHLIB_MAJOR=	0

SSP_SRCS=	fgets_chk.c memcpy_chk.c memmove_chk.c memset_chk.c \
		snprintf_chk.c sprintf_chk.c stpcpy_chk.c stpncpy_chk.c \
		strcat_chk.c strcpy_chk.c strncat_chk.c strncpy_chk.c \
		vsnprintf_chk.c vsprintf_chk.c

.for i in ${SSP_SRCS}
SRCS+=${i}
.endfor

CFLAGS.snprintf_chk.c+=	-Wno-unused-parameter
CFLAGS.sprintf_chk.c+=	-Wno-unused-parameter
CFLAGS.vsnprintf_chk.c+=	-Wno-unused-parameter
CFLAGS.vsprintf_chk.c+=	-Wno-unused-parameter

MAN+=	ssp.3 __builtin_object_size.3

VERSION_DEF=	${.CURDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

.PATH: ${SRCTOP}/lib/libc/secure
CFLAGS+=	-I${SRCTOP}/lib/libc/include
# _elf_aux_info is exported from libc as elf_aux_info(3), so just that for the
# libssp build instead.
CFLAGS+=	-D_elf_aux_info=elf_aux_info
SRCS+=		libc_stack_protector.c

# Stack protection on libssp symbols should be considered harmful, as we may
# be talking about, for example, the guard setup constructor.
SSP_CFLAGS:=

.include <bsd.lib.mk>
