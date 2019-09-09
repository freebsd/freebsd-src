# $FreeBSD$

# ZFS is not supported, we want debugging until this is vetted and
# we don't want the gptboot.efifat thing created.
MK_LOADER_ZFS=no
EFI_DEBUG=yes

BOOT1?=		gptboot
.PATH:		${SRCTOP}/stand/efi/boot1 ${SRCTOP}/stand/libsa
CFLAGS+=	-I${SRCTOP}/stand/efi/boot1
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-DBOOTPROG=\"gptboot.efi\"
SRCS+=		gpt.c
CWARNFLAGS.gpt.c+=	-Wno-sign-compare -Wno-cast-align
WARNS=6
.include "${.CURDIR}/../boot1/Makefile"
