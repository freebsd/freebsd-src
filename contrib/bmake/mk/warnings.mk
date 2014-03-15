# RCSid:
#	$Id: warnings.mk,v 1.7 2009/12/11 17:06:03 sjg Exp $
#
#	@(#) Copyright (c) 2002, Simon J. Gerraty
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

.ifndef _w_cflags

# Any number of warnings sets can be added.
.-include "warnings-sets.mk"

# Modest defaults - put more elaborate sets in warnings-sets.mk
# -Wunused  etc are here so you can set
# W_unused=-Wno-unused etc.
MIN_WARNINGS?= -Wall \
	-Wformat \
	-Wimplicit \
	-Wunused \
	-Wuninitialized 

LOW_WARNINGS?= ${MIN_WARNINGS} -W -Wstrict-prototypes -Wmissing-prototypes
 
MEDIUM_WARNINGS?= ${LOW_WARNINGS} -Werror

HIGH_WARNINGS?= ${MEDIUM_WARNINGS} \
	-Wcast-align \
	-Wcast-qual \
	-Wparentheses \
	-Wpointer-arith \
	-Wmissing-declarations \
	-Wreturn-type \
	-Wswitch \
	-Wwrite-strings

# The two step default makes it easier to test build with different defaults.
DEFAULT_WARNINGS_SET?= MIN
WARNINGS_SET?= ${DEFAULT_WARNINGS_SET}

# If you add sets, besure to list them (you don't have to touch this list).
ALL_WARNINGS_SETS+= MIN LOW MEDIUM HIGH

.if empty(${WARNINGS_SET}_WARNINGS)
.if ${MAKE_VERSION:U0:[1]:C/.*-//} >= 20050530
.BEGIN:	_empty_warnings
_empty_warnings: .PHONY
.else
.BEGIN:
.endif
	@echo "ERROR: Invalid: WARNINGS_SET=${WARNINGS_SET}"
	@echo "ERROR: Try one of: ${ALL_WARNINGS_SETS:O:u}"; exit 1

.endif

# Without -O or if we've set -O0 somewhere - to make debugging more effective,
# we need to turn off -Wuninitialized as otherwise we get a warning that
# -Werror turns into an error.  To be safe, set W_uninitialized blank.
_w_cflags:= ${CFLAGS} ${CPPFLAGS}
.if ${_w_cflags:M-O*} == "" || ${_w_cflags:M-O0} != ""
W_uninitialized=
.endif

.if ${MAKE_VERSION:U0:[1]:C/.*-//} <= 20040118
# This version uses .for loops to avoid a double free bug in old bmake's
# but the .for loops are sensitive to when this file is read.

# first, make a list of all the warning flags - doesn't matter if
# its redundant - we'll sort -u
_all_sets= ${WARNINGS_SET_${MACHINE_ARCH}} ${WARNINGS_SET} ${ALL_WARNINGS_SETS}
_all_warnings= ${WARNINGS} ${_all_sets:O:u:@s@${$s_WARNINGS}@}

# we want to set W_* for each warning so they are easy to turn off.
# :O:u does a sort -u
# using :C allows us to handle -f* -w* etc as well as -W*
.for w in ${_all_warnings:O:u}
${w:C/-(.)/\1_/} ?= $w
.endfor

# Allow for per-target warnings
# Warning: the WARNINGS+= line below, 
# may make your brain hurt - trust me; it works --sjg
# the idea is that you can set WARNINGS_SET[_${MACHINE_ARCH}]=HIGH 
# and use one of
# W_format_mips_foo.o=
# W_format_foo.o=
# to turn off -Wformat for foo.o (on mips only in the first case), or
# W_format_foo.o=-Wformat=2
# for stricter checking.
#
# NOTE: that we force the target extension to be .o
#
.for w in ${WARNINGS_SET_${MACHINE_ARCH}:U${WARNINGS_SET}:@s@${$s_WARNINGS}@:O:u}
WARNINGS+= ${${w:C/-(.)/\1_/}_${MACHINE_ARCH}_${.TARGET:T:R}.o:U${${w:C/-(.)/\1_/}_${.TARGET:T:R}.o:U${${w:C/-(.)/\1_/}_${MACHINE_ARCH}:U${${w:C/-(.)/\1_/}}}}}
.endfor

.else

# .for loops have the [dis]advantage of being evaluated when read,
# so adding to WARNINGS_SET[_${MACHINE_ARCH}] after this file is 
# read has no effect.
# Replacing the above .for loops with the WARNINGS+= below solves that
# but tiggers a double free bug in bmake-20040118 and earlier.
# Don't try and read this too fast!
#
# The first :@ "loop" handles multiple sets in WARNINGS_SET
#
# In the second :@ "loop", the ::?= noise sets W_foo?=-Wfoo etc
# which makes it easy to turn off override individual flags
# (see W_uninitialized above).
# 
# The last bit expands to ${W_foo_${.TARGET:T}:U${W_foo}}
# which is the bit we ultimately want.  It allows W_* to be set on a
# per target basis.
# 
# NOTE: that we force the target extension to be .o
#
WARNINGS+= ${WARNINGS_SET_${MACHINE_ARCH}:U${WARNINGS_SET}:@s@${$s_WARNINGS}@:O:u:@w@${${w:C/-(.)/\1_/}::?=$w} ${${w:C/-(.)/\1_/}_${MACHINE_ARCH}_${.TARGET:T:R}.o:U${${w:C/-(.)/\1_/}_${.TARGET:T:R}.o:U${${w:C/-(.)/\1_/}_${MACHINE_ARCH}:U${${w:C/-(.)/\1_/}}}}}@}

.endif

.ifndef NO_CFLAGS_WARNINGS
# Just ${WARNINGS} should do, but this is more flexible?
CFLAGS+= ${WARNINGS_${.TARGET:T:R}.o:U${WARNINGS}}
.endif

# it is rather silly that g++ blows up on some warning flags
NO_CXX_WARNINGS+= \
	missing-declarations \
	missing-prototypes \
	nested-externs \
	strict-prototypes

.for s in ${SRCS:M*.cc}
.for w in ${NO_CXX_WARNINGS}
W_$w_${s:T:R}.o=
.endfor
.endfor

.endif # _w_cflags
