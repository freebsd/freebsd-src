#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/plugins/kdb/db2/libdb2/include

SRCS+=	db-config.h \
	db.h

CLEANFILES+= db-config.h db.h

db-config.h:	db-config.hin
	@cp ${.ALLSRC} ${.TARGET}

db.h:	db.hin
	@cp ${.ALLSRC} ${.TARGET}
