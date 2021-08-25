# $FreeBSD$

.include <bsd.init.mk>

SHLIB_NAME=	libclang_rt.asan-${CRTARCH}.so
.undef NO_PIC # sorry...

CFLAGS+=	-DASAN_DYNAMIC=1

SRCS+=		asan/asan_activation.cpp
SRCS+=		asan/asan_allocator.cpp
SRCS+=		asan/asan_debugging.cpp
SRCS+=		asan/asan_descriptions.cpp
SRCS+=		asan/asan_errors.cpp
SRCS+=		asan/asan_fake_stack.cpp
SRCS+=		asan/asan_flags.cpp
SRCS+=		asan/asan_globals.cpp
SRCS+=		asan/asan_interceptors.cpp
SRCS+=		asan/asan_interceptors_memintrinsics.cpp
SRCS+=		asan/asan_linux.cpp
SRCS+=		asan/asan_malloc_linux.cpp
SRCS+=		asan/asan_memory_profile.cpp
SRCS+=		asan/asan_new_delete.cpp
SRCS+=		asan/asan_poisoning.cpp
SRCS+=		asan/asan_posix.cpp
SRCS+=		asan/asan_premap_shadow.cpp
SRCS+=		asan/asan_report.cpp
SRCS+=		asan/asan_rtl.cpp
SRCS+=		asan/asan_shadow_setup.cpp
SRCS+=		asan/asan_stack.cpp
SRCS+=		asan/asan_stats.cpp
SRCS+=		asan/asan_suppressions.cpp
SRCS+=		asan/asan_thread.cpp
SRCS+=		interception/interception_linux.cpp
SRCS+=		interception/interception_type_test.cpp
SRCS+=		sanitizer_common/sancov_flags.cpp
SRCS+=		sanitizer_common/sanitizer_allocator.cpp
SRCS+=		sanitizer_common/sanitizer_allocator_checks.cpp
SRCS+=		sanitizer_common/sanitizer_allocator_report.cpp
SRCS+=		sanitizer_common/sanitizer_chained_origin_depot.cpp
SRCS+=		sanitizer_common/sanitizer_common.cpp
SRCS+=		sanitizer_common/sanitizer_common_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_coverage_libcdep_new.cpp
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
SRCS+=		sanitizer_common/sanitizer_persistent_allocator.cpp
SRCS+=		sanitizer_common/sanitizer_platform_limits_freebsd.cpp
SRCS+=		sanitizer_common/sanitizer_platform_limits_linux.cpp
SRCS+=		sanitizer_common/sanitizer_platform_limits_posix.cpp
SRCS+=		sanitizer_common/sanitizer_posix.cpp
SRCS+=		sanitizer_common/sanitizer_posix_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_printf.cpp
SRCS+=		sanitizer_common/sanitizer_procmaps_bsd.cpp
SRCS+=		sanitizer_common/sanitizer_procmaps_common.cpp
SRCS+=		sanitizer_common/sanitizer_stackdepot.cpp
SRCS+=		sanitizer_common/sanitizer_stacktrace.cpp
SRCS+=		sanitizer_common/sanitizer_stacktrace_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_stacktrace_printer.cpp
SRCS+=		sanitizer_common/sanitizer_suppressions.cpp
SRCS+=		sanitizer_common/sanitizer_symbolizer.cpp
SRCS+=		sanitizer_common/sanitizer_symbolizer_libbacktrace.cpp
SRCS+=		sanitizer_common/sanitizer_symbolizer_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_symbolizer_posix_libcdep.cpp
SRCS+=		sanitizer_common/sanitizer_symbolizer_report.cpp
SRCS+=		sanitizer_common/sanitizer_termination.cpp
SRCS+=		sanitizer_common/sanitizer_thread_registry.cpp
SRCS+=		sanitizer_common/sanitizer_tls_get_addr.cpp
SRCS+=		sanitizer_common/sanitizer_type_traits.cpp
SRCS+=		sanitizer_common/sanitizer_unwind_linux_libcdep.cpp
SRCS+=		ubsan/ubsan_diag.cpp
SRCS+=		ubsan/ubsan_flags.cpp
SRCS+=		ubsan/ubsan_handlers.cpp
SRCS+=		ubsan/ubsan_handlers_cxx.cpp
SRCS+=		ubsan/ubsan_init.cpp
SRCS+=		ubsan/ubsan_monitor.cpp
SRCS+=		ubsan/ubsan_type_hash.cpp
SRCS+=		ubsan/ubsan_type_hash_itanium.cpp
SRCS+=		ubsan/ubsan_value.cpp

# Kludge around a bsd.lib.mk shortcoming: when SHLIB_NAME is defined, but LIB
# and LIB_CXX are undefined (which forces building only a shared library),
# there is no way to tell it to use ${CXX} for linking.  Since the files for
# this library are all C++ anyway, simply set CC to CXX.
CC=		${CXX}

.include <bsd.lib.mk>

CXXFLAGS.ubsan_handlers_cxx.cpp=		-frtti
CXXFLAGS.ubsan_type_hash.cpp=		-frtti
CXXFLAGS.ubsan_type_hash_itanium.cpp=	-frtti
