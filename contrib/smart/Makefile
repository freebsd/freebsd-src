#
# Copyright (c) 2016-2021 Chuck Tuffli <chuck@tuffli.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
PROG=	smart
SRCS=	smart.c libsmart.c libsmart_desc.c
SRCS+=	freebsd_dev.c
LIBADD= cam xo
MAN=smart.8
MLINKS=	smart.8 diskhealth.8
#CFLAGS+= -ggdb -O0
CFLAGS+= -DLIBXO
LINKS= ${BINDIR}/smart ${BINDIR}/diskhealth

.include <bsd.prog.mk>
