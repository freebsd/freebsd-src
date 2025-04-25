PACKAGE=runtime
PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c

LIBADD=	m kvm jail xo
.ifdef PS_CHECK_KEYWORDS
CFLAGS+=-DPS_CHECK_KEYWORDS
.endif

.include <bsd.prog.mk>
