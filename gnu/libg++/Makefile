SUBDIR= libg++ genclass

INCLUDEDIRS=	iostream libg++ g++-include

beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/g++ ]; then \
		mkdir ${DESTDIR}/usr/include/g++; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/g++; \
		chmod 755 ${DESTDIR}/usr/include/g++; \
	fi
	@-if [ ! -d ${DESTDIR}/usr/include/g++/gen ]; then \
		mkdir ${DESTDIR}/usr/include/g++/gen; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/g++/gen; \
		chmod 755 ${DESTDIR}/usr/include/g++/gen; \
	fi
	@-if [ ! -d ${DESTDIR}/usr/include/g++/sys ]; then \
		mkdir ${DESTDIR}/usr/include/g++/sys; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/g++/sys; \
		chmod 755 ${DESTDIR}/usr/include/g++/sys; \
	fi
	@-for i in ${INCLUDEDIRS}; do \
		echo installing includes from $$i ; \
		(cd $$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/g++/$$j || \
			install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
				${DESTDIR}/usr/include/g++/$$j; \
		done); \
	done
	@echo installing includes from g++-include/sys
	@(cd g++-include/sys ; for j in *.[ih]; do \
		cmp -s $$j ${DESTDIR}/usr/include/g++/sys/$$j || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			${DESTDIR}/usr/include/g++/sys/$$j; \
	done)
	@echo installing includes from g++-include/gen
	@(cd g++-include/gen ; for j in *.*P; do \
		cmp -s $$j ${DESTDIR}/usr/include/g++/gen/$$j || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			${DESTDIR}/usr/include/g++/gen/$$j; \
	done)

.include <bsd.subdir.mk>
