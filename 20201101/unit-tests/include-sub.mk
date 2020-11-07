# $NetBSD: include-sub.mk,v 1.6 2020/10/25 12:08:53 rillig Exp $

.if ${.INCLUDEDFROMFILE} == "include-main.mk"
.  info sub-before-ok
.else
.  warning sub-before-fail(${.INCLUDEDFROMFILE})
.endif

# As of 2020-09-05, the .for loop is implemented as "including a file"
# with a custom buffer.  Therefore this loop has side effects on these
# variables.
.for i in once
.  if ${.INCLUDEDFROMFILE} == "include-main.mk"
.    info sub-before-for-ok
.  else
.    warning sub-before-for-fail(${.INCLUDEDFROMFILE})
.  endif
.endfor

# To see the variable 'includes' in action:
#
# Breakpoints:
#	Parse_File		at "PtrVector_Push(&includes, curFile)"
#	ParseMessage		at entry
# Watches:
#	((const IFile *[10])(*includes.items))
#	*curFile

.for i in deeply
.  for i in nested
.    for i in include
.include "include-subsub.mk"
.    endfor
.  endfor
.endfor

.if ${.INCLUDEDFROMFILE} == "include-main.mk"
.  info sub-after-ok
.else
.  warning sub-after-fail(${.INCLUDEDFROMFILE})
.endif

.for i in once
.  if ${.INCLUDEDFROMFILE} == "include-main.mk"
.    info sub-after-for-ok
.  else
.    warning sub-after-for-fail(${.INCLUDEDFROMFILE})
.  endif
.endfor
