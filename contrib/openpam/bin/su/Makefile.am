INCLUDES = -I$(top_srcdir)/include

if WITH_PAM_SU
bin_PROGRAMS = su
su_SOURCES = su.c
su_LDADD = $(top_builddir)/lib/libpam.la
endif
