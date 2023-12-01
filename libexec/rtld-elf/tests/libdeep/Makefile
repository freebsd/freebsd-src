
SHLIB?=		deep
SHLIB_MAJOR=	0

LIBDIR=		${TESTSBASE}/libexec/rtld-elf/rtld_deepbind
SHLIBDIR=	${TESTSBASE}/libexec/rtld-elf/rtld_deepbind

SRCS=	libdeep.c

LIBVAL2=	${.OBJDIR}/../libval2
LDFLAGS+=	-L${LIBVAL2} -Wl,-rpath,'$$ORIGIN'
DPADD+=		-lval2
LDADD+=		-lval2

.include <bsd.lib.mk>
