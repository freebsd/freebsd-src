#AUTOMAKE_OPTIONS = foreign 1.9 subdir-objects
NULL =
BUILT_SOURCES =
CLEANFILES =

noinst_LIBRARIES = libunity.a

libunity_a_CFLAGS =                  	\
	-DUNITY_INCLUDE_DOUBLE		\
	$(NULL)

libunity_a_SOURCES =				\
	../libpkgver/colcomp.c			\
	unity.c					\
	unity.h					\
	unity_internals.h			\
	unity_fixture.c				\
	unity_fixture.h				\
	unity_fixture_internals.h		\
	unity_fixture_malloc_overrides.h	\
	$(NULL)

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
