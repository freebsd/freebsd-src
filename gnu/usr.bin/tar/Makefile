# $FreeBSD$

TARDIR=	${.CURDIR}/../../../contrib/tar
.PATH: ${TARDIR}/lib ${TARDIR}/src

PROG=	gtar

.if defined(WITH_GTAR)
SYMLINKS=  ${BINDIR}/gtar ${BINDIR}/tar
MLINKS= gtar.1 tar.1
.endif

SUBDIR=	doc

# libtar SRCS
SRCS=	addext.c argmatch.c backupfile.c basename.c dirname.c error.c \
	exclude.c full-write.c getdate.c getline.c \
	getstr.c hash.c human.c mktime.c modechange.c prepargs.c \
	print-copyr.c quotearg.c safe-read.c save-cwd.c savedir.c unicodeio.c \
	xgetcwd.c xmalloc.c xstrdup.c xstrtoul.c xstrtoumax.c
SRCS+=	buffer.c compare.c create.c delete.c extract.c incremen.c list.c \
	mangle.c misc.c names.c rtapelib.c tar.c update.c

CFLAGS+= -DHAVE_CONFIG_H -I${.CURDIR} -I${TARDIR}/lib -I${TARDIR}/src

# "Rename" tar.1 to gtar.1 so that we can install gtar.1 with
# tar.1 as an optional link (rather than vice versa)
CLEANFILES= gtar.1
gtar.1: tar.1
	cat ${.CURDIR}/tar.1 > ${.TARGET}

.include <bsd.prog.mk>
