# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining NO_WARNS.

.if !defined(NO_WARNS)
. if defined(WARNS)
.  if ${WARNS} > 0
.   if !defined(NO_WERROR)
CFLAGS		+=	-Werror
.   endif
.  endif
.  if ${WARNS} > 1
CFLAGS		+=	-Wall
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CFLAGS		+=	-Wno-uninitialized
.  endif
.  if ${WARNS} > 2
CFLAGS		+=	-W -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
.  endif
.  if ${WARNS} > 3
CFLAGS		+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align
.  endif
. endif

. if defined(FORMAT_AUDIT)
WFORMAT		=	1
. endif
. if defined(WFORMAT)
.  if ${WFORMAT} > 0
CFLAGS		+=	-Wnon-const-format -Wno-format-extra-args
.   if !defined(NO_WERROR)
CFLAGS		+=	-Werror
.   endif
.  endif
. endif
.endif

# Allow user-specified additional warning flags
CFLAGS		+=	${CWARNFLAGS}
