#	$Id$

LIB=		roken
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/roken
SRCS=		k_getpwuid.c k_getpwnam.c signal.c tm2time.c \
		verify.c inaddr2str.c mini_inetd.c get_window_size.c \
		warnerr.c snprintf.c  strlwr.c strnlen.c strtok_r.c strupr.c
NOPIC=		yes

install:

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/roken
