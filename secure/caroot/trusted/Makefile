
BINDIR=		/usr/share/certs/trusted

TRUSTED_CERTS!=	echo ${.CURDIR}/*.pem 2> /dev/null || true

FILES+=	 ${TRUSTED_CERTS}

cleancerts:
	@[ -z "${TRUSTED_CERTS}" ] || rm ${TRUSTED_CERTS}

.include <bsd.prog.mk>
