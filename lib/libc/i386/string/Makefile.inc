#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/i386/string/Makefile.inc,v 1.13.36.1 2010/02/10 00:26:20 kensmith Exp $

MDSRCS+=bcmp.S bcopy.S bzero.S ffs.S index.S memchr.S memcmp.S memcpy.S \
	memmove.S memset.S rindex.S strcat.S strchr.S strcmp.S strcpy.S \
	strlen.S strncmp.S strrchr.S swab.S wcschr.S wcscmp.S wcslen.S \
	wmemchr.S
