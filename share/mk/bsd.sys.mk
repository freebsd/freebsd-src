# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining NO_WARNS.

# for GCC:  http://gcc.gnu.org/onlinedocs/gcc-3.0.4/gcc_3.html#IDX143

.if !defined(NO_WARNS) && ${CC} != "icc"
. if defined(WARNS)
.  if ${WARNS} > 0
CWARNFLAGS	+=	-Wsystem-headers
.   if !defined(NO_WERROR)
CWARNFLAGS	+=	-Werror
.   endif
.  endif
.  if ${WARNS} > 1
CWARNFLAGS	+=	-Wall -Wno-format-y2k
.  endif
.  if ${WARNS} > 2
CWARNFLAGS	+=	-W -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
.  endif
.  if ${WARNS} > 3
CWARNFLAGS	+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align
.  endif
# BDECFLAGS
.  if ${WARNS} > 5
CWARNFLAGS	+=	-Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
.  endif
.  if ${WARNS} > 1 && ${WARNS} < 5
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CWARNFLAGS	+=	-Wno-uninitialized
.  endif
. endif

. if defined(FORMAT_AUDIT)
WFORMAT		=	1
. endif
. if defined(WFORMAT)
.  if ${WFORMAT} > 0
#CWARNFLAGS	+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CWARNFLAGS	+=	-Wformat=2 -Wno-format-extra-args
.   if !defined(NO_WERROR)
CWARNFLAGS	+=	-Werror
.   endif
.  endif
. endif
.endif

# Allow user-specified additional warning flags
CFLAGS		+=	${CWARNFLAGS}
