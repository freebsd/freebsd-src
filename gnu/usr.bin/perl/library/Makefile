# $FreeBSD$

SUBDIR=	B DB_File Dumper Errno Fcntl IO SysV NDBM_File Opcode POSIX \
	SDBM_File Socket attrs re
.if defined(${PERL_THREADED}) && ${PERL_THREADED} == "yes"
SUBDIR+=Thread
.endif

all:	man3pages
	
beforeinstall:
	@cd ${.OBJDIR}/lib ;\
	for i in `find . \! -type d \! -name \*.3.gz` ; do \
		j=`echo $$i|sed -e 's|auto/DynaLoader|mach/auto/DynaLoader|'` ;\
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
			$$i ${DESTDIR}/usr/libdata/perl/${VERSION}/$$j ;\
	done
	@cd ${.OBJDIR}/lib ;\
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		*.3.gz ${DESTDIR}/usr/share/perl/man/man3
	@cd ${.OBJDIR} ;\
	${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		*.h ${DESTDIR}/usr/libdata/perl/${VERSION}/mach/CORE
	@cd ${.OBJDIR} ;\
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		Config.pm ${DESTDIR}/usr/libdata/perl/${VERSION}/mach

CLEANFILES= ext.libs autosplit pod2man pod2man.PL man3pages

.include <bsd.prog.mk>

man3pages: ${DYNALOADER} pod2man
	@cd ${.OBJDIR}/lib ;\
	for i in `find . -name \*.pm | grep -v Functions.pm` ; do \
		j=`echo $$i | sed -e 's|./||' -e 's|/|::|g' -e 's|.pm|.3|'` ;\
		echo Manifying $$j ;\
		${MINIPERL} -I${.OBJDIR}/lib ${.OBJDIR}/pod2man $$i > $$j ;\
		gzip -fn $$j ;\
	done
	@cd ${.OBJDIR}/ext ;\
	for i in `find . -name \*.pm -o -name \*.pod | grep -v POSIX.pm` ; do \
		j=`echo $$i | sed -e 's|./||' -e 's|/SysV/|/IPC/|' \
			-e 's|/Dumper/|/Data/|' -e 's|/lib/|/|' \
			-e 's|^[^/]*/||' \
			-e 's|/|::|g' -e 's|.pm|.3|' -e 's|.pod|.3|'`;\
		i=`echo $$i | sed -e 's|./||'` ;\
		echo Manifying $$j ;\
		${MINIPERL} -I${.OBJDIR}/lib ${.OBJDIR}/pod2man $$i > ../lib/$$j ;\
		gzip -fn ../lib/$$j ;\
	done
	@touch ${.TARGET}

pod2man: scripts autosplit ${PERL5SRC}/pod/pod2man.PL
	ln -sf ${PERL5SRC}/pod/pod2man.PL 
	${MINIPERL} -I${.OBJDIR}/lib pod2man.PL

.PATH:	${PERL5SRC}
