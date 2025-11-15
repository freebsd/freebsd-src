#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include <src.opts.mk>

PLUGINS= 	kdb preauth k5tls
# PLUGINS+=	audit - We don't build this because the port doesn't
#			use it.

SUBDIR=	${PLUGINS}
SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
