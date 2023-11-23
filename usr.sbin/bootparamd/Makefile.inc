
BINDIR?=	/usr/sbin

NO_WCAST_ALIGN=
CWARNFLAGS.clang+=	-Wno-incompatible-pointer-types-discards-qualifiers
CWARNFLAGS.gcc+=	-Wno-error=discarded-qualifiers
