#	$NetBSD: Makefile.inc,v 1.2 2000/10/10 21:51:54 jeffs Exp $
# $FreeBSD: src/lib/libc/mips/string/Makefile.inc,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SRCS+=	bcmp.S bcopy.S bzero.S  ffs.S index.S memchr.c memcmp.c memset.c \
	memcpy.S memmove.S \
	rindex.S strcat.c strcmp.S strcpy.c strcspn.c strlen.S \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
