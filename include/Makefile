#	@(#)Makefile	8.2 (Berkeley) 1/4/94
#
# Doing a make install builds /usr/include
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.
#
all clean cleandir depend lint tags:

FILES=	a.out.h ar.h assert.h bitstring.h ctype.h db.h dirent.h disktab.h \
	err.h fnmatch.h fstab.h fts.h glob.h grp.h kvm.h limits.h locale.h \
	math.h memory.h mp.h mpool.h ndbm.h netdb.h nlist.h paths.h pwd.h \
	ranlib.h regex.h regexp.h resolv.h rune.h runetype.h setjmp.h \
	sgtty.h signal.h stab.h stddef.h stdio.h stdlib.h string.h strings.h \
	struct.h sysexits.h tar.h time.h ttyent.h tzfile.h unistd.h utime.h \
	utmp.h vis.h

MFILES=	float.h frame.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

DIRS=arpa protocols
LDIRS=	net netccitt netinet netiso netns nfs sys ufs vm

NOOBJ=	noobj

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
# SHARED=	copies
SHARED=	symlinks

install: ${SHARED}
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/$$i > /dev/null 2>&1 || \
		    install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		    ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${DIRS}
	@-for i in ${DIRS}; do \
		if [ ! -d ${DESTDIR}/usr/include/$$i ]; \
		then \
			mkdir ${DESTDIR}/usr/include/$$i; \
		fi; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
		chmod 755 ${DESTDIR}/usr/include/$$i; \
		(cd $$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/$$i/$$j > \
			    /dev/null 2>&1 || \
			    install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			    ${DESTDIR}/usr/include/$$i/$$j; \
		done); \
	done
	@echo installing ${LFILES}
	@-for i in ${LFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i; \
		ln -s sys/$$i ${DESTDIR}/usr/include/$$i; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
	done
	@echo installing ${MFILES}
	@-for i in ${MFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i; \
		ln -s machine/$$i ${DESTDIR}/usr/include/$$i; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
	done

copies:
	@echo copies: ${LDIRS}
	@-for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i; \
		cd /sys; \
		tar Hcf - $$i/*.h | (cd ${DESTDIR}/usr/include; tar xpfB -); \
	done
	rm -f ${DESTDIR}/usr/include/machine
	mkdir ${DESTDIR}/usr/include/machine
	cd /sys/${MACHINE}/include && \
	    tar Hcf - *.h | (cd ${DESTDIR}/usr/include/machine; tar xpfB -);
	rm -f ${DESTDIR}/usr/include/X11
	mkdir ${DESTDIR}/usr/include/X11
	cd /usr/X11/include/X11 && \
	    tar Hcf - *.h | (cd ${DESTDIR}/usr/include/X11; tar xpfB -);

symlinks:
	@echo symlinks: ${LDIRS}
	@for i in ${LDIRS}; do \
		rm -rf ${DESTDIR}/usr/include/$$i; \
		ln -s /sys/$$i ${DESTDIR}/usr/include/$$i; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
	done
	rm -f ${DESTDIR}/usr/include/machine
	ln -s ${DESTDIR}/sys/${MACHINE}/include ${DESTDIR}/usr/include/machine
	rm -f ${DESTDIR}/usr/include/X11
	ln -s ${DESTDIR}/usr/X11/include/X11 ${DESTDIR}/usr/include/X11

.include <bsd.prog.mk>
