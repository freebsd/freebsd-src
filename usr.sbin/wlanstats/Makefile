.include <bsd.compiler.mk>

PROG=	wlanstats
MAN=	wlanstats.8

CFLAGS=	-I${SRCTOP}/lib/libbsdstat
LIBADD=	bsdstat

SRCS=	main.c \
	wlanstats.c

CFLAGS.clang+= -fbracket-depth=512 -Wno-cast-align

.include <bsd.prog.mk>
