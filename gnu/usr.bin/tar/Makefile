# $FreeBSD$

TARDIR=	${.CURDIR}/../../../contrib/tar
.PATH: ${TARDIR}/lib ${TARDIR}/src

PROG=	tar

SUBDIR=	doc

# libtar SRCS
SRCS=	addext.c argmatch.c backupfile.c basename.c dirname.c error.c \
	exclude.c full-write.c getdate.c getline.c getopt.c getopt1.c \
	getstr.c hash.c human.c mktime.c modechange.c prepargs.c \
	print-copyr.c quotearg.c safe-read.c save-cwd.c savedir.c unicodeio.c \
	xgetcwd.c xmalloc.c xstrdup.c xstrtoul.c xstrtoumax.c strtoumax.c
SRCS+=	buffer.c compare.c create.c delete.c extract.c incremen.c list.c \
	mangle.c misc.c names.c rtapelib.c tar.c update.c

CFLAGS+= -DHAVE_CONFIG_H -I${.CURDIR} -I${TARDIR}/lib -I${TARDIR}/src

NOSHARED?= YES

.include <bsd.prog.mk>
