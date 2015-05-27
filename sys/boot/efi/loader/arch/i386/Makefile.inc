# $FreeBSD$

SRCS+=	start.S \
	efimd.c \
	elf32_freebsd.c \
	exec.c

.PATH:	${.CURDIR}/../../i386/libi386
SRCS+=	nullconsole.c \
	comconsole.c

CFLAGS+=	-fPIC
LDFLAGS+=	-Wl,-znocombreloc
