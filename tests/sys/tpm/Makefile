.include <src.opts.mk>

TESTSDIR=	${TESTSBASE}/sys/tpm

ATF_TESTS_C+=	tpm_eventlog_test
CFLAGS+=	-I${SRCTOP}/sys

.if ${MK_EFI} != "no"
ATF_TESTS_C+=	efi_eventlog_test

# Compile the loader source using loader/ shims and the real EDK2 headers.
.PATH: ${SRCTOP}/stand/efi/libefi
SRCS.efi_eventlog_test=	efi_eventlog_test.c efitcg.c
EFI_CFLAGS=	-I${.CURDIR}/loader \
		-I${SRCTOP}/stand/efi/include \
		-I${SRCTOP}/sys/contrib/edk2/Include
CFLAGS.efi_eventlog_test.c+= ${EFI_CFLAGS}
CFLAGS.efitcg.c+= ${EFI_CFLAGS}
.endif

.include <bsd.test.mk>
