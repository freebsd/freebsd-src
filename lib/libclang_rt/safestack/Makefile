# $FreeBSD$

.include <bsd.init.mk>

LIB=	clang_rt.safestack-${CRTARCH}

.PATH:	${CRTSRC}/lib/interception\
	${CRTSRC}/lib/sanitizer_common\
	${CRTSRC}/lib/safestack

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
	safestack.cc

.include <bsd.lib.mk>
