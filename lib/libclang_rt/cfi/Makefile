
.include <bsd.init.mk>

LIB=		clang_rt.cfi-${CRTARCH}

SRCS+=		cfi/cfi.cpp
SRCS+=		interception/interception_linux.cpp
SRCS+=		sanitizer_common/sanitizer_allocator.cpp
SRCS+=		sanitizer_common/sanitizer_allocator_checks.cpp
SRCS+=		sanitizer_common/sanitizer_common.cpp
SRCS+=		sanitizer_common/sanitizer_common_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_deadlock_detector1.cpp
SRCS+=		sanitizer_common/sanitizer_dl.cpp
SRCS+=		sanitizer_common/sanitizer_errno.cpp
SRCS+=		sanitizer_common/sanitizer_file.cpp
SRCS+=		sanitizer_common/sanitizer_flag_parser.cpp
SRCS+=		sanitizer_common/sanitizer_flags.cpp
SRCS+=		sanitizer_common/sanitizer_libc.cpp
SRCS+=		sanitizer_common/sanitizer_libignore.cpp
SRCS+=		sanitizer_common/sanitizer_linux.cpp
SRCS+=		sanitizer_common/sanitizer_linux_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_mutex.cpp
SRCS+=		sanitizer_common/sanitizer_platform_limits_freebsd.cpp
SRCS+=		sanitizer_common/sanitizer_posix.cpp
SRCS+=		sanitizer_common/sanitizer_posix_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_printf.cpp
SRCS+=		sanitizer_common/sanitizer_procmaps_bsd.cpp
SRCS+=		sanitizer_common/sanitizer_procmaps_common.cpp
SRCS+=		sanitizer_common/sanitizer_range.cpp
SRCS+=		sanitizer_common/sanitizer_suppressions.cpp
SRCS+=		sanitizer_common/sanitizer_termination.cpp
SRCS+=		sanitizer_common/sanitizer_thread_arg_retval.cpp
SRCS+=		sanitizer_common/sanitizer_thread_registry.cpp
SRCS+=		sanitizer_common/sanitizer_tls_get_addr.cpp
SRCS+=		sanitizer_common/sanitizer_type_traits.cpp

FILESDIR=	${SANITIZER_SHAREDIR}
FILES+=		cfi/cfi_ignorelist.txt

.include <bsd.lib.mk>
