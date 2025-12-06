#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/krb5/ccache

SRCS+=	cc_api_macos.c \
	cc_dir.c \
	cc_file.c \
	cc_kcm.c \
	cc_keyring.c \
	cc_memory.c \
	cc_retr.c \
	ccapi_util.c \
	ccbase.c \
	cccopy.c \
	cccursor.c \
	ccdefault.c \
	ccdefops.c \
	ccfns.c \
	ccmarshal.c \
	ccselect.c \
	ccselect_hostname.c \
	ccselect_k5identity.c \
	ccselect_realm.c
