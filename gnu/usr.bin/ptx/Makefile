PROG=	ptx
SRCS=	argmatch.c diacrit.c error.c getopt.c getopt1.c ptx.c xmalloc.c

LDADD+=	-lgnuregex
DPADD+= ${GNUREGEX}
CFLAGS+= -DHAVE_CONFIG_H -DDEFAULT_IGNORE_FILE=\"/usr/share/dict/eign\"

NOMAN=

.include <bsd.prog.mk>
