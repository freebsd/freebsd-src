#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/builtin/enc_provider

SRCS+=	builtin_rc4.c

CLEANFILES+=	builtin_rc4.c

builtin_rc4.c: ${KRB5_DIR}/lib/crypto/builtin/enc_provider/rc4.c
	@cp ${.ALLSRC} ${.TARGET}
