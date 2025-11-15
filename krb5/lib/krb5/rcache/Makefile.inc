#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/krb5/rcache

SRCS+=	\
	memrcache.c \
	rc_base.c \
	rc_dfl.c \
	rc_file2.c \
	rc_none.c
