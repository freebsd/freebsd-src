.include <bsd.compiler.mk>

PROG=	wlanstat
MAN=	wlanstat.8

CFLAGS=	-I${SRCTOP}/lib/libbsdstat
LIBADD=	bsdstat

SRCS=	main.c \
	wlanstat.c

CFLAGS.clang+= -fbracket-depth=512 -Wno-cast-align

.include <bsd.prog.mk>
