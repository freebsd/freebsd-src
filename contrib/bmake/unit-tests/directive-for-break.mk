# $NetBSD: directive-for-break.mk,v 1.5 2023/06/01 20:56:35 rillig Exp $
#
# Tests for .break in .for loops, which immediately terminates processing of
# the surrounding .for loop.


# .break terminates the loop early.
# This is usually done within a conditional.
.for i in 1 2 3 4 5 6 7 8
.  if $i == 3
I=	$i
.    break
I=	unreached
.  endif
.endfor
.if $I != "3"
.  error
.endif


# The .break only breaks out of the immediately surrounding .for loop, any
# other .for loops are continued normally.
.for outer in o1 o2 o3
.  for inner in i1 i2 i3
.    if ${outer} == o2 && ${inner} == i2
.      break
.    endif
COMBINED+=	${outer}-${inner}
.  endfor
.endfor
# Only o2-i2 and o2-i3 are missing.
.if ${COMBINED} != "o1-i1 o1-i2 o1-i3 o2-i1 o3-i1 o3-i2 o3-i3"
.  error
.endif


# A .break outside the context of a .for loop is an error.
.if $I == 0
# No parse error, even though the .break occurs outside a .for loop, since
# lines from inactive branches are only parsed as far as necessary to see
# whether they belong to an .if/.elif/.else/.endif chain.
.  break
.else
# expect+1: break outside of for loop
.  break
.endif


# Since cond.c 1.335 from 2022-09-02 and before cond.c 1.338 from 2022-09-23,
# the following paragraph generated the wrong error message '4294967294 open
# conditionals'.
.if 1
.  if 2
.    for var in value
.      if 3
.        break
.      endif
.    endfor
.  endif
.endif


.for i in 1
# expect+1: The .break directive does not take arguments
.  break 1
.endfor
