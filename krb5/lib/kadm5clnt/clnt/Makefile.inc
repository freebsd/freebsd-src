#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/kadm5/clnt

SRCS+=	client_init.c \
	client_principal.c \
	client_rpc.c \
	clnt_chpass_util.c \
	clnt_policy.c \
	clnt_privs.c
