#
# Copyright 2014, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

man_MANS = xolint.1

EXTRA_DIST = xolint.1 xolint.pl

install-exec-hook:
	install ${srcdir}/xolint.pl ${bindir}/xolint
