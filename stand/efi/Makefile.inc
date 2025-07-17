.include <src.opts.mk>

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

# GNU or ELF Tool Chain objcopy is usable for building EFI objects.
# LLVM objcopy is not.  Use elfcopy if we've bootstrapped it (and thus
# known to be available) or objcopy is LLVM's.
.if ${MK_ELFTOOLCHAIN_BOOTSTRAP} != "no"
EFI_OBJCOPY?=	elfcopy
.else
_v!=		${OBJCOPY} --version
.if ${_v:Mllvm-objcopy*} == ""
EFI_OBJCOPY?=	${OBJCOPY}
.else
EFI_OBJCOPY?=	elfcopy
.endif
.endif

# Arbitrarily set the PE/COFF header timestamps to 1 Jan 2024 00:00:00
# for build reproducibility.
SOURCE_DATE_EPOCH?=1704067200

.include "../Makefile.inc"
