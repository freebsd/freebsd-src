PROG=	ptx
SRCS=	argmatch.c diacrit.c error.c getopt.c getopt1.c ptx.c xmalloc.c

LDADD+=	-lgnuregex
DPADD+= ${LIBGNUREGEX}
CFLAGS+= -DHAVE_CONFIG_H -DDEFAULT_IGNORE_FILE=\"/usr/share/dict/eign\"

NOMAN=

SUBDIR+= doc

.include <bsd.prog.mk>
