# $NetBSD: forloop.mk,v 1.7 2020/11/03 17:37:57 rillig Exp $

all: for-loop

LIST=	one "two and three" four "five"

.if make(for-fail)
for-fail:

XTRA_LIST=	xtra
.else

.  for x in ${LIST}
.    info x=$x
.  endfor

CFL=	-I/this -I"This or that" -Ithat "-DTHIS=\"this and that\""
cfl=
.  for x in ${CFL}
.    info x=$x
.    if empty(cfl)
cfl=	$x
.    else
cfl+=	$x
.    endif
.  endfor
.  info cfl=${cfl}

.  if ${cfl} != ${CFL}
.    error ${.newline}${cfl} != ${.newline}${CFL}
.  endif

.  for a b in ${EMPTY}
.    info a=$a b=$b
.  endfor

# Since at least 1993, iteration stops at the first newline.
# Back then, the .newline variable didn't exist, therefore it was unlikely
# that a newline ever occurred.
.  for var in a${.newline}b${.newline}c
.    info newline-item=(${var})
.  endfor

.endif	# for-fail

.for a b in ${LIST} ${LIST:tu} ${XTRA_LIST}
.  info a=$a b=$b
.endfor

for-loop:
	@echo We expect an error next:
	@(cd ${.CURDIR} && ${.MAKE} -f ${MAKEFILE} for-fail) && \
	{ echo "Oops that should have failed!"; exit 1; } || echo OK
