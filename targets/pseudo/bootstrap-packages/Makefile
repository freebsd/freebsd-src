# SPDX-License-Identifier: BSD-2-Clause
#
# Compensate (a bit) for the lack of per package makefiles or other means
# of knowing what goes in each package in the base system.
# We can derive some of the information we want from the makefiles that
# set PACKAGE.

all:
.if ${.MAKE.LEVEL} > 0
all: packages
.endif

PACKAGES?= ${.CURDIR:H:H}/packages

packages: package-makefile.list
	@${.CURDIR}/bootstrap-packages.sh ${BOOTSTRAP_PACKAGES_FLAGS} \
	PACKAGES=${PACKAGES} ${.ALLSRC}

package-makefile.list:
	@(cd ${SRCTOP} && \
	find ${TOPS:U*bin etc lib*} -name 'Makefile' | \
	xargs grep '^PACKAGE[[:space:]]*=' ) | \
	sed 's/[[:space:]]*=[[:space:]]*/=/' > ${.TARGET}

