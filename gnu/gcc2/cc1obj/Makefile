PROG=		gcc1obj
BINDIR=		/usr/libexec
SRCS=		objc-parse.c objc-act.c \
		c-lex.c c-pragma.c c-decl.c c-typeck.c c-convert.c \
		c-aux-info.c c-iterate.c
CFLAGS+=	-I. -I$(.CURDIR) -I$(.CURDIR)/../lib
YFLAGS=
NOMAN=		noman
.PATH:		$(.CURDIR)/../cc1

.if exists(${.CURDIR}/../lib/obj)
LDADD=	-L${.CURDIR}/../lib/obj -lgcc2
DPADD=	${.CURDIR}/../lib/obj/libgcc2.a
.else
LDADD=	-L${.CURDIR}/../lib/ -lgcc2
DPADD=	${.CURDIR}/../lib/libgcc2.a
.endif

LDADD+=	-lgnumalloc
DPADD+=	${LIBGNUMALLOC}

.include <bsd.prog.mk>
