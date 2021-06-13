CLANG_SUBDIR=clang/12.0.0
CLANGDIR=	/usr/lib/${CLANG_SUBDIR}
SANITIZER_LIBDIR=		${CLANGDIR}/lib/freebsd

# armv[67] is a bit special since we allow a soft-floating version via
# CPUTYPE matching *soft*. This variant may not actually work though.
.if ${MACHINE_ARCH:Marmv[67]*} != "" && \
    (!defined(CPUTYPE) || ${CPUTYPE:M*soft*} == "")
CRTARCH?=	armhf
.else
CRTARCH?=	${MACHINE_ARCH:S/amd64/x86_64/:C/hf$//:S/mipsn32/mips64/}
.endif

.if ${COMPILER_TYPE} == "clang"
# The only way to set the path to the sanitizer libraries with clang is to
# override the resource directory.
# Note: lib/freebsd is automatically appended to the -resource-dir value.
SANITIZER_LDFLAGS=	-resource-dir=${SYSROOT}${CLANGDIR}
# Also set RPATH to ensure that the dynamically linked runtime libs are found.
SANITIZER_LDFLAGS+=	-Wl,--enable-new-dtags
SANITIZER_LDFLAGS+=	-Wl,-rpath,${SANITIZER_LIBDIR}
.else
.error "Unknown link flags for -fsanitize=... COMPILER_TYPE=${COMPILER_TYPE}"
.endif
