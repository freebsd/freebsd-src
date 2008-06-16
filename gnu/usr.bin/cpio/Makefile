# $FreeBSD$

CPIODIR= ${.CURDIR}/../../../contrib/cpio
.PATH: ${CPIODIR}/lib ${CPIODIR}/src ${CPIODIR}/doc

SUBDIR=	doc

PROG=	gcpio
SRCS=   copyin.c \
	copyout.c \
	copypass.c \
	defer.c \
	dstring.c \
	filemode.c \
	global.c \
	idcache.c \
	main.c \
	makepath.c \
	tar.c \
	userspec.c \
	util.c \
	argp-ba.c \
	argp-eexst.c \
	argp-fmtstream.c \
	argp-fs-xinl.c \
	argp-help.c \
	argp-parse.c \
	argp-pv.c \
	argp-pvh.c \
	argp-xinl.c \
	basename.c \
	dirname.c \
	error.c \
	exitfail.c \
	full-write.c \
	getopt.c \
	getopt1.c \
	mempcpy.c \
	rtapelib.c \
	safe-read.c \
	safe-write.c \
	strchrnul.c \
	stripslash.c \
	strndup.c \
	strnlen.c \
	xalloc-die.c \
	xmalloc.c \
	alloca.h \
	getopt.h

CLEANFILES+= alloca.h getopt.h

getopt.h: getopt_.h
	ln -fs ${.ALLSRC} ${.TARGET}

alloca.h: alloca_.h
	ln -fs ${.ALLSRC} ${.TARGET}

CFLAGS+=-I${.OBJDIR} -I${.CURDIR} -I${CPIODIR}/lib -DHAVE_CONFIG_H \
	-DHAVE_MKFIFO -DHAVE_SETLOCALE -DHAVE_LSTAT

gcpio.1: ${CPIODIR}/doc/cpio.1
	cat ${CPIODIR}/doc/cpio.1 >gcpio.1

SYMLINKS=gcpio ${BINDIR}/cpio
MLINKS=gcpio.1 cpio.1

.include <bsd.prog.mk>
