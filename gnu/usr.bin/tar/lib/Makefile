# $FreeBSD$

LIB=	tar
INTERNALLIB= YES

SRCS=	addext.c argmatch.c backupfile.c basename.c dirname.c error.c \
	exclude.c full-write.c getdate.c getopt.c getopt1.c getstr.c \
	getline.c hash.c human.c mktime.c modechange.c msleep.c prepargs.c \
	print-copyr.c quote.c quotearg.c safe-read.c save-cwd.c savedir.c \
	unicodeio.c xgetcwd.c xmalloc.c xstrdup.c \
	xstrtoimax.c xstrtoul.c xstrtoumax.c

CFLAGS+= ${TARCFLAGS}

.include <bsd.lib.mk>

.PATH: ${TARDIR}/lib
