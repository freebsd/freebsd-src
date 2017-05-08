# $OpenPAM: Makefile.am 938 2017-04-30 21:34:42Z des $

if WITH_TEST

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lib/libpam

AM_TESTS_ENVIRONMENT = \
	PAM_RETURN_SO=$(abs_top_builddir)/modules/pam_return/.libs/pam_return.so

noinst_HEADERS = t_pam_conv.h

# tests
TESTS =
TESTS += t_openpam_ctype
TESTS += t_openpam_dispatch
TESTS += t_openpam_readword
TESTS += t_openpam_readlinev
check_PROGRAMS = $(TESTS)

# libt - common support code
check_LIBRARIES = libt.a
libt_a_SOURCES = t_pam_conv.c

# link with libpam and test framework
LDADD = $(CRYB_TEST_LIBS) libt.a
if WITH_SYSTEM_LIBPAM
LDADD += $(SYSTEM_LIBPAM)
else
LDADD += $(top_builddir)/lib/libpam/libpam.la
endif

endif
