# $FreeBSD$

PROG=		objformat
NOMAN=		not yet
CFLAGS+=	-DMAIN -DROOTDIR=\"${DESTDIR}/usr/libexec/\"
.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
.else
CFLAGS+=	-DFREEBSD_AOUT
.endif

objformat:
	@echo Compiles on install.

install:
	$(CC) ${CFLAGS} ${.CURDIR}/objformat.c -o ${DESTDIR}/usr/bin/objformat

LINKS+= /usr/bin/objformat /usr/bin/ar
LINKS+= /usr/bin/objformat /usr/bin/as
LINKS+= /usr/bin/objformat /usr/bin/ld
LINKS+= /usr/bin/objformat /usr/bin/nm
LINKS+= /usr/bin/objformat /usr/bin/ranlib
LINKS+= /usr/bin/objformat /usr/bin/size
LINKS+= /usr/bin/objformat /usr/bin/strings
LINKS+= /usr/bin/objformat /usr/bin/strip

.include <bsd.prog.mk>
