#	@(#)Makefile	8.2 (Berkeley) 4/19/94

PROG=	config
SRCS=	files.c gram.y hash.c main.c mkheaders.c mkioconf.c mkmakefile.c \
	mkswap.c pack.c scan.l sem.c util.c
CFLAGS+=-I${.CURDIR} -I.
CLEANFILES=gram.c scan.c y.tab.h
MAN8=	config.new.0

MLINKS+=config.new.0 config.0

.include <bsd.prog.mk>

.depend: gram.c scan.c
