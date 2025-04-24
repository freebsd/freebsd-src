
.include <src.opts.mk>

.ifndef LLVM_BASE
.error Please define LLVM_BASE before including this file
.endif

.ifndef LLVM_SRCS
.error Please define LLVM_SRCS before including this file
.endif

.ifndef SRCDIR
.error Please define SRCDIR before including this file
.endif

.ifndef OS_REVISION
.error Please define OS_REVISION before including this file
.endif

.PATH:		${LLVM_BASE}/${SRCDIR}

CFLAGS+=	-I${SRCTOP}/lib/clang/include
CFLAGS+=	-I${LLVM_SRCS}/include
CFLAGS+=	-D__STDC_CONSTANT_MACROS
CFLAGS+=	-D__STDC_FORMAT_MACROS
CFLAGS+=	-D__STDC_LIMIT_MACROS
CFLAGS+=	-DHAVE_VCS_VERSION_INC
.if ${MK_LLVM_ASSERTIONS} == "no"
CFLAGS+=	-DNDEBUG
.endif

# Note that using TARGET_ARCH here is essential for a functional native-xtools
# build!  For native-xtools, we're building binaries that will work on the
# *host* machine (MACHINE_ARCH), but they should default to producing binaries
# for the *target* machine (TARGET_ARCH).
TARGET_ARCH?=	${MACHINE_ARCH}
BUILD_ARCH?=	${MACHINE_ARCH}

# Armv6 and armv7 uses hard float abi, unless the CPUTYPE has soft in it.
# For all other targets, we stick with 'unknown'.
.if ${TARGET_ARCH:Marm*}
.if !defined(CPUTYPE) || ${CPUTYPE:M*soft*} == ""
TARGET_TRIPLE_ABI=-gnueabihf
.else
TARGET_TRIPLE_ABI=-gnueabi
.endif
.else
TARGET_TRIPLE_ABI=
.endif
VENDOR=		unknown

LLVM_TARGET_TRIPLE?=	${TARGET_ARCH:C/amd64/x86_64/}-${VENDOR}-freebsd${OS_REVISION}${TARGET_TRIPLE_ABI}
LLVM_BUILD_TRIPLE?=	${BUILD_ARCH:C/amd64/x86_64/}-${VENDOR}-freebsd${OS_REVISION}

CFLAGS+=	-DLLVM_DEFAULT_TARGET_TRIPLE=\"${LLVM_TARGET_TRIPLE}\"
CFLAGS+=	-DLLVM_HOST_TRIPLE=\"${LLVM_BUILD_TRIPLE}\"
CFLAGS+=	-DDEFAULT_SYSROOT=\"${TOOLS_PREFIX}\"

.if ${MK_LLVM_TARGET_AARCH64} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_AARCH64
. if ${MACHINE_CPUARCH} == "aarch64"
LLVM_NATIVE_ARCH=	AArch64
. endif
.endif
.if ${MK_LLVM_TARGET_ARM} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_ARM
. if ${MACHINE_CPUARCH} == "arm"
LLVM_NATIVE_ARCH=	ARM
. endif
.endif
.if ${MK_LLVM_TARGET_BPF} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_BPF
.endif
.if ${MK_LLVM_TARGET_MIPS} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_MIPS
.endif
.if ${MK_LLVM_TARGET_POWERPC} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_POWERPC
. if ${MACHINE_CPUARCH} == "powerpc"
LLVM_NATIVE_ARCH=	PowerPC
. endif
.endif
.if ${MK_LLVM_TARGET_RISCV} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_RISCV
. if ${MACHINE_CPUARCH} == "riscv"
LLVM_NATIVE_ARCH=	RISCV
. endif
.endif
.if ${MK_LLVM_TARGET_X86} != "no"
CFLAGS+=	-DLLVM_TARGET_ENABLE_X86
. if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
LLVM_NATIVE_ARCH=	X86
. endif
.endif

.ifdef LLVM_NATIVE_ARCH
CFLAGS+=	-DLLVM_NATIVE_ASMPARSER=LLVMInitialize${LLVM_NATIVE_ARCH}AsmParser
CFLAGS+=	-DLLVM_NATIVE_ASMPRINTER=LLVMInitialize${LLVM_NATIVE_ARCH}AsmPrinter
CFLAGS+=	-DLLVM_NATIVE_DISASSEMBLER=LLVMInitialize${LLVM_NATIVE_ARCH}Disassembler
CFLAGS+=	-DLLVM_NATIVE_TARGET=LLVMInitialize${LLVM_NATIVE_ARCH}Target
CFLAGS+=	-DLLVM_NATIVE_TARGETINFO=LLVMInitialize${LLVM_NATIVE_ARCH}TargetInfo
CFLAGS+=	-DLLVM_NATIVE_TARGETMC=LLVMInitialize${LLVM_NATIVE_ARCH}TargetMC
.endif

CFLAGS+=	-ffunction-sections
CFLAGS+=	-fdata-sections
.include "bsd.linker.mk"
.if ${LINKER_TYPE} == "mac"
LDFLAGS+=	-Wl,-dead_strip
.else
LDFLAGS+=	-Wl,--gc-sections
# XXX: --gc-sections strips the ELF brand note and on RISC-V the OS/ABI ends up
# as NONE, so for statically-linked binaries, i.e. lacking an interpreter,
# get_brandinfo finds nothing and (f)execve fails with ENOEXEC. Work around
# this by manually setting the OS/ABI field via the emulation.
.if ${MACHINE_ARCH:Mriscv64*} != "" && ${NO_SHARED:Uno:tl} != "no" && \
    (${.MAKE.OS} == "FreeBSD" || !defined(BOOTSTRAPPING))
LDFLAGS+=	-Wl,-m,elf64lriscv_fbsd
.endif
.endif

CXXSTD=		c++17
CXXFLAGS+=	-fno-exceptions
CXXFLAGS+=	-fno-rtti
.if ${.MAKE.OS} == "FreeBSD" || !defined(BOOTSTRAPPING)
CXXFLAGS.clang+= -stdlib=libc++
.else
# Building on macOS/Linux needs the real sysctl() not the bootstrap tools stub.
CFLAGS+=	-DBOOTSTRAPPING_WANT_NATIVE_SYSCTL
.endif
.if defined(BOOTSTRAPPING) && ${.MAKE.OS} == "Linux"
LIBADD+=	dl
.endif
