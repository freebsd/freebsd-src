# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining NO_WARNS.

# for GCC:  http://gcc.gnu.org/onlinedocs/gcc-3.0.4/gcc_3.html#IDX143

CSTD		?=	c99
.if ${CSTD} == "k&r"
CFLAGS		+=	-traditional
.elif ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS		+=	-std=iso9899:1990
.elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS		+=	-std=iso9899:199409
.elif ${CSTD} == "c99"
CFLAGS		+=	-std=iso9899:1999
.else
CFLAGS		+=	-std=${CSTD}
.endif

.if !defined(NO_WARNS)
. if defined(WARNS)
.  if ${WARNS} > 0
CFLAGS		+=	-Wsystem-headers
.   if !defined(NO_WERROR)
CFLAGS		+=	-Werror
.   endif
.  endif
.  if ${WARNS} > 1
CFLAGS		+=	-Wall -Wno-format-y2k
.  endif
.  if ${WARNS} > 2
CFLAGS		+=	-W -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
.  endif
.  if ${WARNS} > 3
CFLAGS		+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align
.  endif
#	XXX: the warning that was here became problematic.
#.  if ${WARNS} > 4
# BDECFLAGS
.  if ${WARNS} > 5
.   if defined(CSTD)
CFLAGS		+=	-pedantic
.   endif
CFLAGS		+=	-Wbad-function-cast -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
.  endif
.  if ${WARNS} > 1 && ${WARNS} < 5
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CFLAGS		+=	-Wno-uninitialized
.  endif
. endif

. if defined(FORMAT_AUDIT)
WFORMAT		=	1
. endif
. if defined(WFORMAT)
.  if ${WFORMAT} > 0
#CFLAGS		+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CFLAGS		+=	-Wformat=2 -Wno-format-extra-args
.   if !defined(NO_WERROR)
CFLAGS		+=	-Werror
.   endif
.  endif
. endif
.endif

# Allow user-specified additional warning flags
CFLAGS		+=	${CWARNFLAGS}
