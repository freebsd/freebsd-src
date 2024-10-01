#
# libc sources related to security

.PATH: ${LIBC_SRCTOP}/secure

# _FORTIFY_SOURCE
SRCS+=	fgets_chk.c memcpy_chk.c memmove_chk.c mempcpy_chk.c memset_chk.c \
	snprintf_chk.c sprintf_chk.c stpcpy_chk.c stpncpy_chk.c \
	strcat_chk.c strcpy_chk.c strlcat_chk.c strncat_chk.c strlcpy_chk.c \
	strncpy_chk.c vsnprintf_chk.c vsprintf_chk.c

CFLAGS.snprintf_chk.c+=	-Wno-unused-parameter
CFLAGS.sprintf_chk.c+=	-Wno-unused-parameter
CFLAGS.vsnprintf_chk.c+=	-Wno-unused-parameter
CFLAGS.vsprintf_chk.c+=	-Wno-unused-parameter

# Sources common to both syscall interfaces:
SRCS+=	libc_stack_protector.c \
	stack_protector_compat.c

SYM_MAPS+=    ${LIBC_SRCTOP}/secure/Symbol.map
