# $FreeBSD$

DEV=		lj4
LJ4RES=		1200
LJ4PRINT=	${PSPRINT}
DEVFILES=	DESC \
		AB ABI AI AR \
		ALBB ALBR AOB AOI AOR CB CBI CI CR GB GBI GI GR \
		LGB LGI LGR OB OBI OI OR TB TBI TI TR \
		TNRB TNRBI TNRI TNRR \
		UB UBI UI UR \
		UCB UCBI UCI UCR CLARENDON CORONET MARIGOLD S \
		SYMBOL WINGDINGS

CLEANFILES+=	DESC

DESC: DESC.in
	echo "res ${LJ4RES}" >${.TARGET}
	echo "unitwidth `expr 7620000 / ${LJ4RES}`" >>${.TARGET}
	cat ${.ALLSRC} >>${.TARGET}
	if test "${PAGE}" = A4; then \
	  echo "papersize a4" >>${.TARGET}; \
	else \
	  echo "papersize letter" >>${.TARGET}; \
	fi
	test -z '${LJ4PRINT}' || echo print '${LJ4PRINT}' >>${.TARGET}

.include "../Makefile.dev"
