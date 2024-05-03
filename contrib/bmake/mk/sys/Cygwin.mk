# Minimal adjustments for Cygwin
# SPDX-License-Identifier: BSD-2-Clause

OS ?=		Cygwin
unix ?=		We run ${OS}.

.ifndef ROOT_GROUP
# Cygwin maps local admin SID S-1-5-32-544 to GID 544.
# /etc/group does no longer exist in a base installation.
ROOT_GROUP != /usr/bin/getent group 544 2>/dev/null
ROOT_GROUP := ${ROOT_GROUP:C,:.*$,,}
.endif

.LIBS:		.a

AR ?=		ar
RANLIB ?=	ranlib
TSORT ?=	tsort -q

# egrep is deprecated
EGREP ?=	grep -E
