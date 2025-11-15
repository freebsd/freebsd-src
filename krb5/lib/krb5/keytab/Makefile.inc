#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/krb5/keytab

SRCS+=	kt_file.c \
	kt_memory.c \
	ktadd.c \
	ktbase.c \
	ktdefault.c \
	ktfns.c \
	ktfr_entry.c \
	ktremove.c \
	read_servi.c
