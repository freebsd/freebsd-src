# $FreeBSD$

PROG_CXX=	groff
SRCS=		groff.cc pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.if ${MACHINE_ARCH} == "alpha"
#XXXX ld produces only one PT_LOAD segment when this binary
#XXXX is linked dynamically, which confuses the rtld
LDFLAGS+= 	-static
.endif

.include <bsd.prog.mk>
