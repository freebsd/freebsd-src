SUBDIR= libg++ genclass

INCLUDEDIRS=	iostream libg++

beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/g++ ]; then \
		mkdir ${DESTDIR}/usr/include/g++; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/g++; \
		chmod 755 ${DESTDIR}/usr/include/g++; \
	fi
	@-for i in ${INCLUDEDIRS}; do \
		echo installing includes from $$i ; \
		(cd $$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/g++/$$j || \
			install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
				${DESTDIR}/usr/include/g++/$$j; \
		done); \
	done
	@(cd g++-include ; for j in *.*P; do \
		cmp -s $$j ${DESTDIR}/usr/include/g++/$$j || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			${DESTDIR}/usr/include/g++/$$j; \
	done)
	@(cd include ; for j in *.h; do \
		cmp -s $$j ${DESTDIR}/usr/include/$$j || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			${DESTDIR}/usr/include/$$j; \
	done)

.include <bsd.subdir.mk>
