# $Id: auto.obj.mk,v 1.8 2011/08/08 17:35:20 sjg Exp $
#
#	@(#) Copyright (c) 2004, Simon J. Gerraty
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

ECHO_TRACE ?= echo

.ifndef Mkdirs
# A race condition in some versions of mkdir, means that it can bail 
# if another process made a dir that mkdir expected to.
# We repeat the mkdir -p a number of times to try and work around this.
# We stop looping as soon as the dir exists.
# If we get to the end of the loop, a plain mkdir will issue an error.
Mkdirs= Mkdirs() { \
	for d in $$*; do \
		for i in 1 2 3 4 5 6; do \
			mkdir -p $$d; \
			test -d $$d && return 0; \
		done > /dev/null 2>&1; \
		mkdir $$d || exit $$?; \
	done; }
.endif

# if MKOBJDIRS is set to auto (and NOOBJ isn't defined) do some magic...
# This will automatically create objdirs as needed.
# Skip it if we are just doing 'clean'.
.if !defined(NOOBJ) && !defined(NO_OBJ) && ${MKOBJDIRS:Uno} == auto
# Use __objdir here so it is easier to tweak without impacting
# the logic.
__objdir?= ${MAKEOBJDIR}
.if ${.OBJDIR} != ${__objdir}
# We need to chdir, make the directory if needed
.if !exists(${__objdir}/) && \
	(${.TARGETS} == "" || ${.TARGETS:Nclean*:N*clean:Ndestroy*} != "")
# This will actually make it... 
__objdir_made != echo ${__objdir}/; umask ${OBJDIR_UMASK:U002}; \
        ${ECHO_TRACE} "[Creating objdir ${__objdir}...]" >&2; \
        ${Mkdirs}; Mkdirs ${__objdir}
.endif
# This causes make to use the specified directory as .OBJDIR
.OBJDIR: ${__objdir}
.if ${.OBJDIR} != ${__objdir} && ${__objdir_made:Uno:M${__objdir}/*} != ""
.error could not use ${__objdir}
.endif
.endif
.endif
