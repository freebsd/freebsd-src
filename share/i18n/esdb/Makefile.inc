
.include <bsd.own.mk>

.PATH: ${.CURDIR}

ESDBDIR?= /usr/share/i18n/esdb
# mkesdb builds as part of bootstrap-tools and can therefore be found in $PATH
MKESDB?=	mkesdb
