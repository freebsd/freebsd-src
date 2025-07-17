.PATH: ${SRCTOP}/contrib/com_err

PACKAGE=	kerberos

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l
.if defined(BOOTSTRAPPING)
# compiler_et does not need the full libroken but just a bunch of the file
# in there, the buildsystem we have will trigger the full dependency chain
# when linking statically including libcrypt, which is a ldscript, not
# supported by macOS ar(1).
LIBROKEN_A=	${.OBJDIR:H:H}/kerberos5/lib/libroken/libroken.a
LDADD=	${LIBROKEN_A}
DPADD=	${LIBROKEN_A}
.else
LIBADD=	roken
.endif
LIBADD+=	vers
CFLAGS+=-I. -I${SRCTOP}/contrib/com_err

WARNS?=	0

.include <bsd.prog.mk>
