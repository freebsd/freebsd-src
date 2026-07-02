PROG=	fand
MAN=	fand.8
SRCS=	fand.c config.c
INCS=	fand.h

# Will limit sysctl sensors to the ones having CTLFLAG_CAPRD.
# CFLAGS+=	-DUSE_CAPSICUM

LIBADD=	util figpar

.include <bsd.prog.mk>
