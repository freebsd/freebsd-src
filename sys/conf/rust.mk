# Common Rust settings for FreeBSD kernel and kernel module builds.
#
# Rust kernel support is intentionally opt-in while the ABI and driver
# patterns settle. Source files are compiled directly with rustc; Cargo is not
# part of the kernel build.

.if !target(__<rust.mk>__)
__<rust.mk>__:	.NOTMAIN

RUSTC?=		rustc

RUST_TARGET.amd64?=	x86_64-unknown-freebsd
RUST_KERNEL_TARGET?=	${RUST_TARGET.${MACHINE_ARCH}:U}

RUST_OPT_LEVEL?=	2
RUST_DEBUG_LEVEL?=	0

RUST_KERNEL_FLAGS= \
	--edition=2021 \
	--crate-type lib \
	--emit=obj \
	-C panic=abort \
	-C opt-level=${RUST_OPT_LEVEL} \
	-C debuginfo=${RUST_DEBUG_LEVEL} \
	-C relocation-model=static \
	-C force-frame-pointers=yes

RUST_KERNEL_FLAGS.amd64= \
	-C code-model=kernel \
	-C no-redzone=yes \
	-C target-feature=-mmx,-sse,-sse2,-sse3,-ssse3,-sse4.1,-sse4.2,-avx,-avx2

.if ${MK_RUST_KERNEL} != "no"
.if empty(RUST_KERNEL_TARGET)
.error "WITH_RUST_KERNEL is only supported on amd64 initially"
.endif
RUSTC_VERSION!=	command -v ${RUSTC} >/dev/null 2>&1 && ${RUSTC} --version || true
.if empty(RUSTC_VERSION)
.error "WITH_RUST_KERNEL requires rustc in PATH or RUSTC=/path/to/rustc"
.endif
RUSTC_TARGET_OK!=	libdir=`${RUSTC} --target=${RUST_KERNEL_TARGET} --print target-libdir 2>/dev/null`; ls "$$libdir"/libcore-*.rlib >/dev/null 2>&1 && echo yes || true
.if ${RUSTC_TARGET_OK} != "yes"
.error "WITH_RUST_KERNEL requires rustc target ${RUST_KERNEL_TARGET}; install it with 'rustup target add ${RUST_KERNEL_TARGET}' or provide an equivalent toolchain"
.endif
.else
NORMAL_RS=	@echo "Rust kernel support is disabled; rebuild with WITH_RUST_KERNEL=yes" && false
.endif

.if ${MK_RUST_KERNEL} != "no"
NORMAL_RS=	${RUSTC} --target=${RUST_KERNEL_TARGET} ${RUST_KERNEL_FLAGS} \
	${RUST_KERNEL_FLAGS.${MACHINE_CPUARCH}} -o ${.TARGET} ${.IMPSRC}
.endif

.endif
