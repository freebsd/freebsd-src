# SPDX-License-Identifier: BSD-2-Clause
#
#	$Id: Generic.mk,v 1.21 2024/02/17 17:26:57 sjg Exp $
#

.LIBS:		.a

tools  ?= \
	ar \
	lorder \
	ranlib \
	tsort

.for t in ${tools}
${t:tu} := ${t:${M_whence}}
.endfor

# need to make sure this is set
MACHINE_ARCH.${MACHINE} ?= ${MACHINE}
.if empty(MACHINE_ARCH)
MACHINE_ARCH = ${MACHINE_ARCH.${MACHINE}}
.endif

.if !empty(TSORT)
TSORT += -q
.endif


