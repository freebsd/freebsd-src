# $FreeBSD$

PROG=		objformat
NOMAN=		not yet
CFLAGS+=	-DMAIN

.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
.else
CFLAGS+=	-DFREEBSD_AOUT
.endif

# LINKS+=	/usr/bin/objformat /usr/bin/as
# LINKS+=	/usr/bin/objformat /usr/bin/ld

.include <bsd.prog.mk>
