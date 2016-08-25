NO_SYSCALLS=yes

LIB=c_nosyscalls
SHLIB_LDSCRIPT=
SHLIB_LDSCRIPT_LINKS=

LIBC_SRCTOP=	${.CURDIR}/../libc/

# Unbreak shared CHERI builds by disabling them here
NO_SHARED=yes

.include "${LIBC_SRCTOP}/Makefile"

MAN:=
MLINKS:=
