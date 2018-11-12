# $FreeBSD$

SRCS+=	amd64_tramp.S \
	start.S \
	framebuffer.c \
	elf64_freebsd.c \
	trap.c \
	exc.S

.PATH:	${.CURDIR}/../../i386/libi386
SRCS+=	nullconsole.c \
	comconsole.c

CFLAGS+=	-fPIC
LDFLAGS+=	-Wl,-znocombreloc
