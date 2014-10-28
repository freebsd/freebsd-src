#
# Copyright 2014, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

if LIBXO_WARNINGS_HIGH
LIBXO_WARNINGS = HIGH
endif
if HAVE_GCC
GCC_WARNINGS = yes
endif
include ${top_srcdir}/warnings.mk

libxoincdir = ${includedir}/libxo

AM_CFLAGS = -I${top_srcdir} ${WARNINGS}

lib_LTLIBRARIES = libxo.la

libxoinc_HEADERS = \
     xo.h

libxo_la_SOURCES = \
    libxo.c

man_MANS = \
    libxo.3 \
    xo_attr.3 \
    xo_create.3 \
    xo_emit.3 \
    xo_err.3 \
    xo_finish.3 \
    xo_flush.3 \
    xo_format.5 \
    xo_no_setlocale.3 \
    xo_open_container.3 \
    xo_open_list.3 \
    xo_parse_args.3 \
    xo_set_allocator.3 \
    xo_set_flags.3 \
    xo_set_info.3 \
    xo_set_options.3 \
    xo_set_style.3 \
    xo_set_writer.3

EXTRA_DIST =
