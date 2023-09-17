
.include <bsd.init.mk>

LIB=		clang_rt.xray-${CRTARCH}

SRCS+=		sanitizer_common/sanitizer_allocator.cpp
SRCS+=		sanitizer_common/sanitizer_allocator_checks.cpp
SRCS+=		sanitizer_common/sanitizer_common.cpp
SRCS+=		sanitizer_common/sanitizer_common_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_deadlock_detector1.cpp
SRCS+=		sanitizer_common/sanitizer_deadlock_detector2.cpp
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
SRCS+=		sanitizer_common/sanitizer_platform_limits_linux.cpp
SRCS+=		sanitizer_common/sanitizer_platform_limits_posix.cpp
SRCS+=		sanitizer_common/sanitizer_posix.cpp
SRCS+=		sanitizer_common/sanitizer_posix_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_printf.cpp
SRCS+=		sanitizer_common/sanitizer_procmaps_bsd.cpp
SRCS+=		sanitizer_common/sanitizer_procmaps_common.cpp
SRCS+=		sanitizer_common/sanitizer_suppressions.cpp
SRCS+=		sanitizer_common/sanitizer_termination.cpp
SRCS+=		sanitizer_common/sanitizer_thread_registry.cpp
SRCS+=		sanitizer_common/sanitizer_tls_get_addr.cpp
SRCS+=		sanitizer_common/sanitizer_type_traits.cpp
SRCS+=		xray/xray_buffer_queue.cpp
SRCS+=		xray/xray_flags.cpp
SRCS+=		xray/xray_init.cpp
SRCS+=		xray/xray_interface.cpp
SRCS+=		xray/xray_log_interface.cpp
SRCS+=		xray/xray_utils.cpp

.if ${MACHINE_CPUARCH} == amd64
SRCS+=		xray/xray_trampoline_x86_64.S
SRCS+=		xray/xray_x86_64.cpp
.endif # amd64

.if ${MACHINE_ARCH} == powerpc64le
SRCS+=		xray/xray_powerpc64.cpp
SRCS+=		xray/xray_trampoline_powerpc64.cpp
SRCS+=		xray/xray_trampoline_powerpc64_asm.S
.endif # powerpc64le

.PATH:		${CRTSRC}/include/xray
INCSDIR=	${CLANGDIR}/include/xray
INCS+=		xray_interface.h
INCS+=		xray_log_interface.h
INCS+=		xray_records.h

.include <bsd.lib.mk>
