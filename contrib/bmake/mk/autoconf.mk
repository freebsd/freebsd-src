# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: autoconf.mk,v 1.20 2024/02/17 17:26:57 sjg Exp $
#
#	@(#) Copyright (c) 1996-2024, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.NOPATH:	config.h config.gen config.recheck config.status 

CONFIGURE_DEPS += ${.CURDIR}/config.h.in ${.CURDIR}/configure

.if !target(config.h)
config.h:	.NOTMAIN ${CONFIGURE_DEPS} config.status
	./config.status
.if !empty(AUTOCONF_GENERATED_MAKEFILE) && ${AUTOCONF_GENERATED_MAKEFILE:T:@m@${"${.MAKE.MAKEFILES:T:M$m}":?yes:no}@:Mno} != ""
	@echo Generated ${AUTOCONF_GENERATED_MAKEFILE}, you need to restart; exit 1
.endif
.endif

.if !target(config.status)
# avoid the targets behaving differently
config.status:	.NOTMAIN
.if exists(${.OBJDIR}/config.status)
config.status:	config.recheck
.else
config.status:  config.gen
.endif

beforebuild: .NOTMAIN config.status .WAIT

config.recheck: .NOTMAIN ${CONFIGURE_DEPS} config.gen
	./config.status --recheck
	@touch $@

config.gen: .NOTMAIN ${CONFIGURE_DEPS}
	CC="${CC} ${CCMODE}" ${.CURDIR}/configure --no-create ${CONFIGURE_ARGS}
	@touch $@ config.recheck

CLEANFILES+= config.recheck config.gen config.status *.meta \
	${AUTOCONF_GENERATED_MAKEFILE:U}
.endif

# avoid things blowing up if these are not here...
# this is not quite per the autoconf manual,
# and is extremely convoluted - but all utterly necessary!

.if make(autoconf-input) || make(configure) || make(config.h.in) || ${MK_AUTO_AUTOCONF:Uno} == "yes"
AUTOCONF ?= autoconf
AUTOHEADER ?= autoheader

# expand it to a full path
AUTOCONF := ${AUTOCONF:${M_whence}}

.if exists(${AUTOCONF})

.PRECIOUS: configure config.h.in config.status

ACLOCAL =
ACCONFIG =

.if exists(${.CURDIR}/aclocal.m4)
ACLOCAL += aclocal.m4
.endif
# use of acconfig.h is deprecated!
.if exists(${.CURDIR}/acconfig.h)
ACCONFIG += acconfig.h
.endif
.if exists(${.CURDIR}/configure.ac)
CONFIGURE_SRC = ${.CURDIR}/configure.ac
.else
CONFIGURE_SRC ?= ${.CURDIR}/configure.in
.endif

config.h.in:	.NOTMAIN ${CONFIGURE_SRC} ${ACCONFIG}
	(cd ${.CURDIR} && ${AUTOHEADER})

configure:	.NOTMAIN ${CONFIGURE_SRC} ${ACLOCAL}
	(cd ${.CURDIR} && ${AUTOCONF})

AUTOCONF_INPUTS += configure
.if exists(${.CURDIR}/config.h.in)
AUTOCONF_INPUTS += config.h.in
.endif

autoconf-input:	.NOTMAIN ${AUTOCONF_INPUTS}

.endif
.endif
