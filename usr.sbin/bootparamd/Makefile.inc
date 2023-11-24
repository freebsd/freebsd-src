#	@(#)Makefile.inc	5.1 (Berkeley) 5/11/90

BINDIR?=	/usr/sbin

NO_WCAST_ALIGN=
CWARNFLAGS.clang+=	-Wno-incompatible-pointer-types-discards-qualifiers
CWARNFLAGS.gcc+=	-Wno-error=discarded-qualifiers
