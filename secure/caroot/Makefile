CLEANFILES+=	certdata.txt

SUBDIR+=	trusted
SUBDIR+=	untrusted

.include <bsd.obj.mk>

# Set this to an upstream hash or tag
# https://hg-edge.mozilla.org/projects/nss/tags
HGVER = e71e3de47d4ca7a3efa7c11096ab2e20ae71683e

# To be used by secteam@ to update the trusted certificates
fetchcerts: .PHONY
	fetch --mirror -o certdata.txt 'https://hg-edge.mozilla.org/projects/nss/raw-file/${HGVER}/lib/ckfw/builtins/certdata.txt'

cleancerts: .PHONY
	@${MAKE} -C ${.CURDIR}/trusted ${.TARGET}
	@${MAKE} -C ${.CURDIR}/untrusted ${.TARGET}

updatecerts: .PHONY cleancerts fetchcerts
	perl ${.CURDIR}/ca-extract.pl -i certdata.txt \
	    -t ${.CURDIR}/trusted -u ${.CURDIR}/untrusted
