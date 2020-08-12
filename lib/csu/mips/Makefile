# $FreeBSD$

.PATH: ${.CURDIR:H}/common

.include <src.opts.mk>

CFLAGS+=	-DCRT_IRELOC_SUPPRESS

.if ${MACHINE_ARCH:Mmips64}
ELFCLASS=	64
.else
ELFCLASS=	32
.endif
.if ${MACHINE_ARCH:Mmips*el}
ENDIAN=		l
.else
ENDIAN=		b
.endif
.if ${MACHINE_ARCH:Mmipsn32*}
SUFFIX=		n32
.else
SUFFIX=
.endif

# binutils requires an explicit emulation for ld -r
LDFLAGS.bfd+=	-Wl,-m -Wl,elf${ELFCLASS}${ENDIAN}tsmip${SUFFIX}_fbsd

.include <bsd.lib.mk>
