# Options used when building app-specific efi components
# See conf/kern.mk for the correct set of these
CFLAGS+=	-Wformat
LDFLAGS+=	-nostdlib

.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-fshort-wchar
CFLAGS+=	-mno-red-zone
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-fshort-wchar
CFLAGS+=	-fPIC
.endif

.if ${MACHINE_CPUARCH} == "arm"
CFLAGS+=	-fPIC
.endif

.if ${MACHINE_CPUARCH} == "amd64"
.if ${DO32:U0} == 1
EFI_TARGET=	efi-app-ia32
.else
EFI_TARGET=	efi-app-x86_64
.endif
.else
EFI_TARGET=	binary
.endif

# Arbitrarily set the PE/COFF header timestamps to 1 Jan 2024 00:00:00
# for build reproducibility.
SOURCE_DATE_EPOCH?=1704067200

.include "../Makefile.inc"
