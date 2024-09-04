all: mul divrem

MUL=	mul.c ../muldi3.c
mul: ${MUL}
	gcc -g -DSPARC_XXX ${MUL} -o ${.TARGET}

DIVREM=	divrem.c ../qdivrem.c
divrem: ${DIVREM}
	gcc -g -DSPARC_XXX ${DIVREM} -o ${.TARGET}
