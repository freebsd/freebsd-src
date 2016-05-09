# $FreeBSD$

.include <bsd.init.mk>

SHLIB_NAME=	libclang_rt.asan-${CRTARCH}.so
.undef NO_PIC # sorry...

.PATH:	${CRTSRC}/lib/asan\
	${CRTSRC}/lib/interception\
	${CRTSRC}/lib/sanitizer_common\
	${CRTSRC}/lib/lsan\
	${CRTSRC}/lib/ubsan

SRCS=	interception_linux.cc\
	interception_mac.cc\
	interception_win.cc\
	interception_type_test.cc\
	sanitizer_allocator.cc\
	sanitizer_common.cc\
	sanitizer_deadlock_detector1.cc\
	sanitizer_deadlock_detector2.cc\
	sanitizer_flags.cc\
	sanitizer_flag_parser.cc\
	sanitizer_libc.cc\
	sanitizer_libignore.cc\
	sanitizer_linux.cc\
	sanitizer_mac.cc\
	sanitizer_persistent_allocator.cc\
	sanitizer_platform_limits_linux.cc\
	sanitizer_platform_limits_posix.cc\
	sanitizer_posix.cc\
	sanitizer_printf.cc\
	sanitizer_procmaps_common.cc\
	sanitizer_procmaps_freebsd.cc\
	sanitizer_procmaps_linux.cc\
	sanitizer_procmaps_mac.cc\
	sanitizer_stackdepot.cc\
	sanitizer_stacktrace.cc\
	sanitizer_stacktrace_printer.cc\
	sanitizer_suppressions.cc\
	sanitizer_symbolizer.cc\
	sanitizer_symbolizer_libbacktrace.cc\
	sanitizer_symbolizer_mac.cc\
	sanitizer_symbolizer_win.cc\
	sanitizer_tls_get_addr.cc\
	sanitizer_thread_registry.cc\
	sanitizer_win.cc\
	sanitizer_common_libcdep.cc\
	sanitizer_coverage_libcdep.cc\
	sanitizer_coverage_mapping_libcdep.cc\
	sanitizer_linux_libcdep.cc\
	sanitizer_posix_libcdep.cc\
	sanitizer_stacktrace_libcdep.cc\
	sanitizer_stoptheworld_linux_libcdep.cc\
	sanitizer_symbolizer_libcdep.cc\
	sanitizer_symbolizer_posix_libcdep.cc\
	sanitizer_unwind_linux_libcdep.cc\
	lsan_common.cc\
	lsan_common_linux.cc\
	ubsan_diag.cc\
	ubsan_init.cc\
	ubsan_flags.cc\
	ubsan_handlers.cc\
	ubsan_value.cc\
	asan_allocator.cc\
	asan_activation.cc\
	asan_debugging.cc\
	asan_fake_stack.cc\
	asan_flags.cc\
	asan_globals.cc\
	asan_interceptors.cc\
	asan_linux.cc\
	asan_mac.cc\
	asan_malloc_linux.cc\
	asan_malloc_mac.cc\
	asan_malloc_win.cc\
	asan_poisoning.cc\
	asan_posix.cc\
	asan_report.cc\
	asan_rtl.cc\
	asan_stack.cc\
	asan_stats.cc\
	asan_suppressions.cc\
	asan_thread.cc\
	asan_win.cc\
	asan_new_delete.cc\
	ubsan_handlers_cxx.cc\
	ubsan_type_hash.cc\
	ubsan_type_hash_itanium.cc\
	ubsan_type_hash_win.cc

# Kludge around a bsd.lib.mk shortcoming: when SHLIB_NAME is defined, but LIB
# and LIB_CXX are undefined (which forces building only a shared library),
# there is no way to tell it to use ${CXX} for linking.  Since the files for
# this library are all C++ anyway, simply set CC to CXX.
CC=		${CXX}

.include <bsd.lib.mk>

CXXFLAGS.ubsan_handlers_cxx.cc=		-frtti
CXXFLAGS.ubsan_type_hash.cc=		-frtti
CXXFLAGS.ubsan_type_hash_itanium.cc=	-frtti
CXXFLAGS.ubsan_type_hash_win.cc=	-frtti
