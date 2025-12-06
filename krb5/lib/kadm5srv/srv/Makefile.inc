#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/kadm5/srv

SRCS+=	adb_xdr.c \
	kadm5_hook.c \
	pwqual.c \
	pwqual_dict.c \
	pwqual_empty.c \
	pwqual_hesiod.c \
	pwqual_princ.c \
	server_init.c \
	server_kdb.c \
	server_misc.c \
	svr_chpass_util.c \
	svr_iters.c \
	svr_policy.c \
	svr_principal.c
