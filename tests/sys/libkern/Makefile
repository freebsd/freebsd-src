.include <src.opts.mk>

PACKAGE=	tests

.PATH: ${SRCTOP}/sys/libkern

TESTSDIR=	${TESTSBASE}/sys/libkern

ATF_TESTS_C+=	crc32
SRCS.crc32+=	crc32.c
SRCS.crc32+=	gsb_crc32.c
CFLAGS.crc32+=	-DTESTING
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
.PATH: ${SRCTOP}/sys/libkern/x86
SRCS.crc32+=	crc32_sse42.c
.elif ${MACHINE_CPUARCH} == "aarch64"
.PATH: ${SRCTOP}/sys/libkern/arm64
SRCS.crc32+=	crc32c_armv8.S
.endif

ATF_TESTS_C+=	sign_extend_bitfields

WARNS?=	3

.include <bsd.test.mk>
