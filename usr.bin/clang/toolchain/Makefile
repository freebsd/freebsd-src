.include <src.opts.mk>

.if ${MK_LLVM_BINUTILS} == yes
PACKAGE=	toolchain

SYMLINKS+=	llvm-ar ${BINDIR}/ar
SYMLINKS+=	llvm-ranlib ${BINDIR}/ranlib
MLINKS+=	llvm-ar.1 ar.1 llvm-ar.1 ranlib.1

SYMLINKS+=	llvm-cov ${BINDIR}/gcov
MLINKS=		llvm-cov.1 gcov.1

SYMLINKS+=	llvm-cxxfilt ${BINDIR}/c++filt
MLINKS+=	llvm-cxxfilt.1 c++filt.1

SYMLINKS+=	llvm-nm ${BINDIR}/nm
MLINKS+=	llvm-nm.1 nm.1

SYMLINKS+=	llvm-objcopy ${BINDIR}/objcopy
SYMLINKS+=	llvm-strip ${BINDIR}/strip
MLINKS+=	llvm-objcopy.1 objcopy.1
MLINKS+=	llvm-objcopy.1 strip.1

SYMLINKS+=	llvm-objdump ${BINDIR}/objdump
MLINKS=		llvm-objdump.1 objdump.1

SYMLINKS+=	llvm-readelf ${BINDIR}/readelf
MLINKS+=	llvm-readelf.1 readelf.1

SYMLINKS+=	llvm-size ${BINDIR}/size
MLINKS+=	llvm-size.1 size.1

SYMLINKS+=	llvm-addr2line ${BINDIR}/addr2line
MLINKS+=	llvm-addr2line.1 addr2line.1
.endif

.include <bsd.prog.mk>
