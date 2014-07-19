common_utils_cflags = -I$(top_srcdir)/include \
					-I$(top_srcdir)/src \
					-I$(top_srcdir)/uthash
common_utils_ldadd = $(top_builddir)/src/libucl.la

ucl_chargen_SOURCES = chargen.c
ucl_chargen_LDADD = $(common_utils_ldadd)
ucl_chargen_CFLAGS = $(common_utils_cflags)

ucl_objdump_SOURCES = objdump.c
ucl_objdump_LDADD = $(common_utils_ldadd)
ucl_objdump_CFLAGS = $(common_utils_cflags)

if UTILS
UTL = ucl_chargen ucl_objdump
else
UTL =
endif
bin_PROGRAMS = $(UTL)