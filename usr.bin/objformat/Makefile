# $FreeBSD$

PROG=		objformat
NOMAN=		not yet
CFLAGS+=	-DMAIN

.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
.else
CFLAGS+=	-DFREEBSD_AOUT
.endif

LINKS+= /usr/bin/objformat /usr/bin/ar
LINKS+= /usr/bin/objformat /usr/bin/as
LINKS+= /usr/bin/objformat /usr/bin/ld
LINKS+= /usr/bin/objformat /usr/bin/nm
LINKS+= /usr/bin/objformat /usr/bin/ranlib
LINKS+= /usr/bin/objformat /usr/bin/size
LINKS+= /usr/bin/objformat /usr/bin/strings
LINKS+= /usr/bin/objformat /usr/bin/strip

.include <bsd.prog.mk>
