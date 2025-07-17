AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lib/libpam

noinst_PROGRAMS = openpam_dump_policy
openpam_dump_policy_SOURCES = openpam_dump_policy.c
if WITH_SYSTEM_LIBPAM
openpam_dump_policy_LDADD = $(SYSTEM_LIBPAM)
else
openpam_dump_policy_LDADD = $(top_builddir)/lib/libpam/libpam.la
endif
