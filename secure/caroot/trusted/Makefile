BINDIR=		/usr/share/certs/trusted

TRUSTED_CERTS!=	(cd ${.CURDIR} && echo *.pem)

FILES+=	 ${TRUSTED_CERTS}

cleancerts: .PHONY
	@(cd ${.CURDIR} && rm -f ${TRUSTED_CERTS})

.include <bsd.prog.mk>
