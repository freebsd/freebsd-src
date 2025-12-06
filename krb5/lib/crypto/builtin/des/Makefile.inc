#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/builtin/des

SRCS+=	d3_aead.c \
	d3_kysched.c \
	f_aead.c \
	f_cksum.c \
	f_parity.c \
	f_sched.c \
	f_tables.c \
	key_sched.c \
	weak_key.c

# 	des_keys.c
