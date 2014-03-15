
X=a b c d e

.for x in $X
LIB${x:tu}=/tmp/lib$x.a
.endfor

X_LIBS= ${LIBA} ${LIBD} ${LIBE}

LIB?=a

var = head
res = no
.if !empty(var:M${:Uhead\:tail:C/:.*//})
res = OK
.endif

all:
	@for x in $X; do ${.MAKE} -f ${MAKEFILE} show LIB=$$x; done
	@echo "Mscanner=${res}"

show:
	@echo 'LIB=${LIB} X_LIBS:M$${LIB$${LIB:tu}} is "${X_LIBS:M${LIB${LIB:tu}}}"'
	@echo 'LIB=${LIB} X_LIBS:M*/lib$${LIB}.a is "${X_LIBS:M*/lib${LIB}.a}"'
	@echo 'LIB=${LIB} X_LIBS:M*/lib$${LIB}.a:tu is "${X_LIBS:M*/lib${LIB}.a:tu}"'
