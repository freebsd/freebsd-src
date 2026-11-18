.include <src.opts.mk>

TESTSDIR=	${TESTSBASE}/stand/efi

.if ${MK_EFI} != "no"
ATF_TESTS_C+=	tpm_test
LIBADD.tpm_test+=	md

# Exercise the loader source with mocked firmware and allocation failures.
CFLAGS+=	-I${.CURDIR}/include
CFLAGS+=	-I${SRCTOP}/sys
CFLAGS+=	-I${SRCTOP}/stand/efi/libefi
CFLAGS+=	-I${SRCTOP}/stand/efi/include
CFLAGS+=	-I${SRCTOP}/sys/contrib/edk2/Include
.endif

.include <bsd.test.mk>
