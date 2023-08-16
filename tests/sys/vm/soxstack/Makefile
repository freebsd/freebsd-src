
SHLIB=		soxstack
SHLIB_NAME=	libsoxstack.so
SHLIB_MAJOR=	1

WITHOUT_STATIC=
WITHOUT_PROFILE=
WITHOUT_PIC=

SRCS=		soxstack.c
LDFLAGS+=	-Wl,-z,execstack
LIBADD+=	procstat

LIBDIR=		${TESTSBASE}/sys/vm/stack

.include <bsd.lib.mk>
