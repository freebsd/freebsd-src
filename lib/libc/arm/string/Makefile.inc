#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/arm/string/Makefile.inc,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

MDSRCS+=bcmp.c bcopy.S bzero.S ffs.S index.c memchr.c memcmp.S memcpy.S \
	memmove.S memset.S rindex.c strcat.c strchr.c strcmp.S strcpy.c \
	strlen.S strncmp.S strrchr.c swab.c wcschr.c wcscmp.c wcslen.c \
	wmemchr.c
