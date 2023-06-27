# $Id: sys.vars.mk,v 1.15 2023/05/16 16:41:52 sjg Exp $
#
#	@(#) Copyright (c) 2003-2023, Simon J. Gerraty
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

# We use the following paradigm for preventing multiple inclusion.
# It relies on the fact that conditionals and dependencies are resolved
# at the time they are read.
#
# _this ?= ${.PARSEDIR:tA}/${.PARSEFILE}
# .if !target(__${_this}__)
# __${_this}__: .NOTMAIN
#

# if this is an ancient version of bmake
MAKE_VERSION ?= 0
.if ${MAKE_VERSION:M*make-*}
# turn it into what we want - just the date
MAKE_VERSION := ${MAKE_VERSION:[1]:C,.*-,,}
.endif

.if ${MAKE_VERSION} < 20100414
_this = ${.PARSEDIR}/${.PARSEFILE}
.else
_this = ${.PARSEDIR:tA}/${.PARSEFILE}
.endif

# some useful modifiers

# A useful trick for testing multiple :M's against something
# :L says to use the variable's name as its value - ie. literal
# got = ${clean* destroy:${M_ListToMatch:S,V,.TARGETS,}}
M_ListToMatch = L:@m@$${V:U:M$$m}@
# match against our initial targets (see above)
M_L_TARGETS = ${M_ListToMatch:S,V,_TARGETS,}

# turn a list into a set of :N modifiers
# NskipFoo = ${Foo:${M_ListToSkip}}
M_ListToSkip= O:u:S,^,N,:ts:

# type should be a builtin in any sh since about 1980,
# but sadly there are exceptions!
.if ${.MAKE.OS:Unknown:NBSD/OS} == ""
_type_sh = which
.endif

# AUTOCONF := ${autoconf:L:${M_whence}}
M_type = @x@(${_type_sh:Utype} $$x) 2> /dev/null; echo;@:sh:[0]:N* found*:[@]:C,[()],,g
M_whence = ${M_type}:M/*:[1]

# produce similar output to jot(1) or seq(1)
# eg. ${LIST:[#]:${M_JOT}}
# would be 1 2 3 4 5 if LIST has 5 words
# ${9:L:${M_JOT}}
# would be 1 2 3 4 5 6 7 8 9
.if ${.MAKE.LEVEL} == 0
.for x in jot seq
.if empty(JOT_CMD)
JOT_CMD := ${$x:L:${M_whence}}
.endif
.endfor
.if !empty(JOT_CMD)
.export JOT_CMD
.endif
.endif
.if !empty(JOT_CMD)
M_JOT = [1]:S,^,${JOT_CMD} ,:sh
.else
M_JOT = [1]:@x@i=1;while [ $$$$i -le $$x ]; do echo $$$$i; i=$$$$((i + 1)); done;@:sh
.endif

# ${LIST:${M_RANGE}} is 1 2 3 4 5 if LIST has 5 words
.if ${MAKE_VERSION} < 20170130
M_RANGE = [#]:${M_JOT}
.else
M_RANGE = range
.endif

# convert a path to a valid shell variable
M_P2V = tu:C,[./-],_,g

# convert path to absolute
.if ${MAKE_VERSION} < 20100414
M_tA = C,.*,('cd' & \&\& 'pwd') 2> /dev/null || echo &,:sh
.else
M_tA = tA
.endif

# absoulte path to what we are reading.
_PARSEDIR = ${.PARSEDIR:${M_tA}}

.if ${MAKE_VERSION} >= 20170130
# M_cmpv allows comparing dotted versions like 3.1.2
# ${3.1.2:L:${M_cmpv}} -> 3001002
# we use big jumps to handle 3 digits per dot:
# ${123.456.789:L:${M_cmpv}} -> 123456789
M_cmpv.units = 1 1000 1000000 1000000000 1000000000000
M_cmpv = S,., ,g:C,^0*([0-9]),\1,:_:range:@i@+ $${_:[-$$i]} \* $${M_cmpv.units:[$$i]}@:S,^,expr 0 ,1:sh
.endif

# many projects use MAJOR MINOR PATCH versioning
# ${OPENSSL:${M_M.M.P_VERSION}} is equivalent to
# ${OPENSSL_MAJOR_VERSION}.${OPENSSL_MINOR_VERSION}.${OPENSSL_PATCH_VERSION}
M_M.M.P_VERSION = L:@v@$${MAJOR MINOR PATCH:L:@t@$${$$v_$$t_VERSION:U0}@}@:ts.

# numeric sort
.if ${MAKE_VERSION} < 20210803
M_On = O
M_Onr = O
.else
M_On = On
M_Onr = Onr
.endif

# Index of a word in a list.
# eg. ${LIST:${M_Index:S,K,key,}} is the index of
# the word "key" in ${LIST}, of course any pattern can be used.
# If "key" appears more than once, there will be multiple
# index values use ${M_Index:S,K,key,}:[1] to select only the first.
M_Index = _:${M_RANGE}:@i@$${"$${_:[$$i]:MK}":?$$i:}@

# mtime of each word - assumed to be a valid pathname
.if ${.MAKE.LEVEL} < 20230510
M_mtime = tW:S,^,${STAT:Ustat} -f %m ,:sh
.else
# M_mtime_fallback can be =error to throw an error
# or =0 to use 0, default is to use current time
M_mtime = mtime${M_mtime_fallback:U}
.endif

