#	$OpenBSD: Makefile.inc,v 1.1 1995/03/20 14:45:47 mycroft Exp $

SRCS+=	bcmp.S bcopy.S bzero.S  ffs.S index.S memchr.c memcmp.c memset.c \
	rindex.S strcat.c strcmp.S strcpy.c strcspn.c strlen.S \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c rindex.c strcmp.c strlen.c
