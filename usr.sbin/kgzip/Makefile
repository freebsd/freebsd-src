# $FreeBSD$

MAINTAINER=	rnordier

PROG=	kgzip
SRCS=	kgzip.c aouthdr.c elfhdr.c kgzcmp.c kgzld.c xio.c
MAN8=	kgzip.8
CFLAGS+=-pedantic \
	-W -Wall -Waggregate-return -Wbad-function-cast -Wcast-align \
	-Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
	-Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes \
	-Wwrite-strings

.include <bsd.prog.mk>
