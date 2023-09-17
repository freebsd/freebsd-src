#
# $Id$
#
# Copyright 2014, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

doc docs: xolint-errors.rst html

#
# The contents of xolint.rst is generated based on xolint.pl, since we
# really want this to be self-documenting.  But readthedocs.org needs this
# data to be _in_ repo.  So we generate this file on command only, and
# the developer needs to commit any changes.
#

xolint-errors.rst: ${top_srcdir}/xolint/xolint.pl
	perl ${top_srcdir}/xolint/xolint.pl -D > ${top_srcdir}/doc/xolint-errors.rst

SPHINX = python3 -msphinx

html sphinx sphinx-html:
	${SPHINX} -M html ${srcdir} . -N -E

singlehtml:
	${SPHINX} -M singlehtml ${srcdir} . -N -E
