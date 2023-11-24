
.PATH:	${SRCTOP}/sys/dev/xz
.PATH:	${SRCTOP}/sys/contrib/xz-embedded/freebsd
.PATH:	${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz

KMOD=	xz
SRCS= \
	xz_crc32.c \
	xz_crc64.c \
	xz_dec_bcj.c \
	xz_dec_lzma2.c \
	xz_dec_stream.c \
	xz_mod.c

CFLAGS+= \
	-DXZ_USE_CRC64 \
	-I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/include/linux

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
