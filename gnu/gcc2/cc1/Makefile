#	@(#)Makefile	6.2 (Berkeley) 2/2/91

PROG=		gcc1
BINDIR=		/usr/libexec
SRCS=		c-parse.c c-lang.c  c-lex.c c-pragma.c c-decl.c \
		c-typeck.c c-convert.c c-aux-info.c c-iterate.c 
	
CFLAGS+=	-I. -I$(.CURDIR) -I$(.CURDIR)/../lib
YFLAGS=
NOMAN=		noman

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
