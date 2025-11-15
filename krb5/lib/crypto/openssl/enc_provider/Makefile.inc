#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/openssl/enc_provider

SRCS+=	openssl_aes.c \
	openssl_camellia.c \
	openssl_des3.c

CLEANFILES=	openssl_aes.c openssl_camellia.c openssl_des3.c

openssl_aes.c: ${KRB5_DIR}/lib/crypto/openssl/enc_provider/aes.c
	@cp ${.ALLSRC} ${.TARGET}

openssl_camellia.c: ${KRB5_DIR}/lib/crypto/openssl/enc_provider/camellia.c
	@cp ${.ALLSRC} ${.TARGET}

openssl_des3.c: ${KRB5_DIR}/lib/crypto/openssl/enc_provider/des3.c
	@cp ${.ALLSRC} ${.TARGET}
